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

	while (_running) {
		int pollCount = poll(&_pollFds[0], _pollFds.size(), -1);

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
	}
}

void Server::stop() {
	_running = false;

	// Close all client connections
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		close(it->first);
		delete it->second;
	}
	_clients.clear();

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

Client* Server::getClientByNickname(const std::string& nickname) {
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second->getNickname() == nickname)
			return it->second;
	}
	return NULL;
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
	if (!client || message.empty())
		return;

	if (client->getSendBuffer().size() + message.size() > MAX_SEND_BUFFER_SIZE) {
		std::cerr << "Warning: send buffer exceeded for fd " << client->getFd() << std::endl;
		closeConnection(client->getFd(), "Send buffer exceeded");
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
		pfd.events = POLLIN | POLLOUT;
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

	if (bytes < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return;
		std::cerr << "Error: recv failed: " << strerror(errno) << std::endl;
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
		if (!current || !current->hasCompleteMessage())
			break;

		std::string message = current->extractMessage();
		if (!message.empty())
			processClientMessage(current, message);
	}
}

void Server::handleClientSend(int fd) {
	Client* client = getClient(fd);
	if (!client || client->getSendBuffer().empty())
		return;

	const std::string& buffer = client->getSendBuffer();
	ssize_t bytes = send(fd, buffer.c_str(), buffer.size(), 0);

	if (bytes < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			std::cerr << "Error: send failed: " << strerror(errno) << std::endl;
			closeConnection(fd, "Send error");
		}
		return;
	}

	client->clearSendBuffer(bytes);
}

void Server::closeConnection(int fd, const std::string& reason) {
	close(fd);
	removeClient(fd, reason);
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
