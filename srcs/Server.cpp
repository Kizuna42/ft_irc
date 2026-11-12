#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "CommandHandler.hpp"
#include "Message.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <set>
#include <stdexcept>

volatile sig_atomic_t Server::s_shutdownRequested = 0;

static const int POLL_TIMEOUT_MS = 1000;

Server::Server(int port, const std::string& password)
	: _serverFd(-1), _port(port), _password(password), _commandHandler(NULL), _running(false) {
	_commandHandler = new CommandHandler(this);
}

Server::~Server() {
	stop();
	delete _commandHandler;
}

// Server operations
void Server::start() {
	setupSocket();
	_running = true;

	std::cout << "IRC Server started on port " << _port << std::endl;

	while (_running && !s_shutdownRequested) {
		updatePollEvents();
		// Finite timeout: a signal delivered between the loop-condition check
		// and poll() does not interrupt poll(), so re-check the flag periodically.
		int pollCount = poll(&_pollFds[0], _pollFds.size(), POLL_TIMEOUT_MS);

		if (pollCount < 0) {
			if (errno == EINTR)
				continue;
			std::cerr << "Error: poll failed: " << strerror(errno) << std::endl;
			break;
		}

		for (size_t i = 0; i < _pollFds.size(); ) {
			struct pollfd current = _pollFds[i];
			if (current.revents == 0) {
				++i;
				continue;
			}

			bool closed = false;

			if (current.fd == _serverFd) {
				if (current.revents & POLLIN)
					handleNewConnection();
				++i;
				continue;
			}

			// Already scheduled for disconnection: ignore further events.
			if (isClosePending(current.fd)) {
				++i;
				continue;
			}

			if (current.revents & POLLIN) {
				handleClientData(current.fd);
				if (!getClient(current.fd))
					closed = true;
			}

			if (!closed && (current.revents & POLLOUT)) {
				handleClientSend(current.fd);
				if (!getClient(current.fd))
					closed = true;
			}

			if (!closed && (current.revents & (POLLERR | POLLHUP | POLLNVAL))) {
				closeConnection(current.fd, "Poll error");
				closed = true;
			}

			if (!closed)
				++i;
		}

		flushPendingCloses();
	}

	if (s_shutdownRequested)
		std::cout << "\nShutting down server..." << std::endl;
}

void Server::requestShutdown() {
	s_shutdownRequested = 1;
}

void Server::stop() {
	_running = false;

	// Close all client connections
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		close(it->first);
		delete it->second;
	}
	_clients.clear();
	_pendingClose.clear();

	// Delete all channels
	for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
		delete it->second;
	}
	_channels.clear();

	// Close server socket
	if (_serverFd >= 0) {
		close(_serverFd);
		_serverFd = -1;
	}

	_pollFds.clear();
}

bool Server::isRunning() const {
	return _running;
}

// Client management
Client* Server::getClient(int fd) {
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end())
		return it->second;
	return NULL;
}

// Nicknames are compared case-insensitively (RFC 1459 section 2.2).
Client* Server::getClientByNickname(const std::string& nickname) {
	if (nickname.empty())
		return NULL;
	const std::string folded = ircToLower(nickname);
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (ircToLower(it->second->getNickname()) == folded)
			return it->second;
	}
	return NULL;
}

// Pending invitations are keyed by nickname, so carry them over on NICK.
void Server::renameInvites(const std::string& oldNick, const std::string& newNick) {
	if (oldNick.empty())
		return;
	for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
		if (it->second->isInvited(oldNick)) {
			it->second->removeInvite(oldNick);
			it->second->addInvite(newNick);
		}
	}
}

void Server::removeClient(int fd, const std::string& reason) {
	Client* client = getClient(fd);
	if (!client)
		return;

	std::string quitReason = reason.empty() ? "Client disconnected" : reason;
	std::string quitMsg = ":" + client->getPrefix() + " QUIT :" + quitReason + "\r\n";
	broadcastToClientChannels(client, quitMsg, client);

	// Remove from all channels
	for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ) {
		Channel* channel = it->second;
		if (channel->isMember(client)) {
			channel->removeMember(client);
		}

		// Remove empty channels
		if (channel->getMemberCount() == 0) {
			delete channel;
			_channels.erase(it++);
		} else {
			++it;
		}
	}

	// Remove from client list
	_clients.erase(fd);
	delete client;

	// Remove from poll fds
	for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it) {
		if (it->fd == fd) {
			_pollFds.erase(it);
			break;
		}
	}
}

void Server::disconnectClient(Client* client, const std::string& reason) {
	if (!client)
		return;
	closeConnection(client->getFd(), reason);
}

void Server::sendToClient(Client* client, const std::string& message) {
	if (!client || message.empty() || isClosePending(client->getFd()))
		return;

	if (client->getSendBuffer().size() + message.size() > MAX_SEND_BUFFER_SIZE) {
		std::cerr << "Warning: send buffer exceeded for fd " << client->getFd() << std::endl;
		// Do not close here: callers may be iterating over channel members
		// or the channel map. The connection is closed after this poll cycle.
		scheduleClose(client->getFd(), "Send buffer exceeded");
		return;
	}

	client->appendSendBuffer(message);
}

// Channel management
Channel* Server::getChannel(const std::string& name) {
	std::map<std::string, Channel*>::iterator it = _channels.find(name);
	if (it != _channels.end())
		return it->second;
	return NULL;
}

Channel* Server::createChannel(const std::string& name) {
	Channel* channel = getChannel(name);
	if (!channel) {
		channel = new Channel(name, this);
		_channels[name] = channel;
	}
	return channel;
}

void Server::removeChannel(const std::string& name) {
	std::map<std::string, Channel*>::iterator it = _channels.find(name);
	if (it != _channels.end()) {
		delete it->second;
		_channels.erase(it);
	}
}

void Server::broadcastToClientChannels(Client* client, const std::string& message, Client* exclude) {
	if (!client)
		return;
	std::set<Client*> delivered;
	if (exclude)
		delivered.insert(exclude);
	for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
		Channel* channel = it->second;
		if (!channel->isMember(client))
			continue;
		const std::vector<Client*>& members = channel->getMembers();
		for (std::vector<Client*>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
			Client* member = *mit;
			if (delivered.insert(member).second)
				sendToClient(member, message);
		}
	}
}

// Getters
const std::string& Server::getPassword() const {
	return _password;
}

// Socket operations
void Server::setupSocket() {
	// Create socket
	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverFd < 0)
		throw std::runtime_error("Error: socket creation failed");

	// Set socket options
	int opt = 1;
	if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		close(_serverFd);
		throw std::runtime_error("Error: setsockopt failed");
	}

	// Set non-blocking
	setNonBlocking(_serverFd);

	// Bind
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(_port);

	if (bind(_serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(_serverFd);
		throw std::runtime_error("Error: bind failed");
	}

	// Listen
	if (listen(_serverFd, SOMAXCONN) < 0) {
		close(_serverFd);
		throw std::runtime_error("Error: listen failed");
	}

	// Add to poll
	struct pollfd pfd;
	pfd.fd = _serverFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollFds.push_back(pfd);
}

void Server::handleNewConnection() {
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);

	int clientFd = accept(_serverFd, (struct sockaddr*)&clientAddr, &clientLen);
	if (clientFd < 0) {
		std::cerr << "Error: accept failed: " << strerror(errno) << std::endl;
		return;
	}

	try {
		setNonBlocking(clientFd);
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		close(clientFd);
		return;
	}

	// Create client
	try {
		Client* client = new Client(clientFd);
		client->setHostname(inet_ntoa(clientAddr.sin_addr));
		_clients[clientFd] = client;

		// Add to poll
		struct pollfd pfd;
		pfd.fd = clientFd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_pollFds.push_back(pfd);

		std::cout << "New connection from " << inet_ntoa(clientAddr.sin_addr) << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "Error: Failed to create client (memory allocation failed): " << e.what() << std::endl;
		close(clientFd);
	}
}

void Server::handleClientData(int fd) {
	Client* client = getClient(fd);
	if (!client)
		return;

	char buffer[512];
	ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

	// poll() reported this fd readable, so -1 is treated as a failed
	// connection. errno is intentionally not inspected after recv().
	if (bytes < 0) {
		std::cerr << "Error: recv failed (fd: " << fd << ")" << std::endl;
		closeConnection(fd, "Read error");
		return;
	}

	if (bytes == 0) {
		std::cout << "Client disconnected (fd: " << fd << ")" << std::endl;
		closeConnection(fd, "Connection closed");
		return;
	}

	buffer[bytes] = '\0';
	client->appendRecvBuffer(std::string(buffer, bytes));

	// Process complete messages
	while (true) {
		Client* current = getClient(fd);
		if (!current || isClosePending(fd) || !current->hasCompleteMessage())
			break;

		std::string message = current->extractMessage();
		if (!message.empty())
			processClientMessage(current, message);
	}

	// Guard against a client that keeps sending data without a line break.
	Client* current = getClient(fd);
	if (current && current->getRecvBuffer().size() > MAX_RECV_BUFFER_SIZE) {
		std::cerr << "Warning: receive buffer exceeded for fd " << fd << std::endl;
		closeConnection(fd, "Input buffer exceeded");
	}
}

void Server::handleClientSend(int fd) {
	Client* client = getClient(fd);
	if (!client || client->getSendBuffer().empty())
		return;

	const std::string& buffer = client->getSendBuffer();
	ssize_t bytes = send(fd, buffer.c_str(), buffer.size(), 0);

	// errno is intentionally not inspected after send(): on -1 the data stays
	// buffered and is retried on the next POLLOUT. A broken socket is reported
	// by poll() as POLLERR/POLLHUP and closed by the event loop.
	if (bytes < 0)
		return;

	client->clearSendBuffer(bytes);
}

void Server::closeConnection(int fd, const std::string& reason) {
	_pendingClose.erase(fd);
	close(fd);
	removeClient(fd, reason);
}

void Server::scheduleClose(int fd, const std::string& reason) {
	if (_pendingClose.find(fd) == _pendingClose.end())
		_pendingClose[fd] = reason;
}

bool Server::isClosePending(int fd) const {
	return _pendingClose.find(fd) != _pendingClose.end();
}

void Server::flushPendingCloses() {
	// Closing a connection broadcasts QUIT, which may schedule more closes.
	while (!_pendingClose.empty()) {
		std::map<int, std::string>::iterator it = _pendingClose.begin();
		int fd = it->first;
		std::string reason = it->second;
		_pendingClose.erase(it);
		if (getClient(fd))
			closeConnection(fd, reason);
	}
}

void Server::updatePollEvents() {
	// Only ask for POLLOUT when there is pending output; otherwise poll()
	// would return immediately for every writable socket and spin the CPU.
	for (size_t i = 0; i < _pollFds.size(); ++i) {
		if (_pollFds[i].fd == _serverFd)
			continue;
		Client* client = getClient(_pollFds[i].fd);
		_pollFds[i].events = POLLIN;
		if (client && !client->getSendBuffer().empty())
			_pollFds[i].events |= POLLOUT;
	}
}

// Utility
void Server::setNonBlocking(int fd) {
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error("Error: failed to set non-blocking mode");
}

void Server::processClientMessage(Client* client, const std::string& message) {
	Message msg(message);
	if (!msg.getCommand().empty())
		_commandHandler->execute(client, msg);
}
