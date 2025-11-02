#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Message.hpp"

void CommandHandler::handlePass(Client* client, const Message& msg) {
	// Check if already registered
	if (client->isAuthenticated()) {
		_server->sendToClient(client, createReply(ERR::ALREADYREGISTRED, client->getNickname(), ":You may not reregister"));
		return;
	}

	// Check parameters
	const std::vector<std::string>& params = msg.getParams();
	std::string password = params.empty() ? msg.getTrailing() : params[0];

	if (password.empty()) {
		_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, "*", "PASS :Not enough parameters"));
		return;
	}

	// Verify password
	if (password == _server->getPassword()) {
		client->setPassword(true);
	} else {
		_server->sendToClient(client, createReply(ERR::PASSWDMISMATCH, "*", ":Password incorrect"));
		return;
	}

	// Check if registration is complete
	if (client->hasPassword() && client->hasNick() && client->hasUser()) {
		client->setAuthenticated(true);
		std::string welcome = createReply(RPL::WELCOME, client->getNickname(),
			":Welcome to the Internet Relay Network " + client->getPrefix());
		_server->sendToClient(client, welcome);
	}
}

void CommandHandler::handleNick(Client* client, const Message& msg) {
	// Check parameters
	const std::vector<std::string>& params = msg.getParams();
	if (params.empty()) {
		_server->sendToClient(client, createReply(ERR::NONICKNAMEGIVEN, "*", ":No nickname given"));
		return;
	}

	std::string newNick = params[0];

	// Validate nickname
	if (!isValidNickname(newNick)) {
		_server->sendToClient(client, createReply(ERR::ERRONEUSNICKNAME, "*", newNick + " :Erroneous nickname"));
		return;
	}

	// Check if nickname is already in use
	Client* existingClient = _server->getClientByNickname(newNick);
	if (existingClient && existingClient != client) {
		_server->sendToClient(client, createReply(ERR::NICKNAMEINUSE, "*", newNick + " :Nickname is already in use"));
		return;
	}

	// Set nickname
	std::string oldNick = client->getNickname();
	std::string oldPrefix = client->getPrefix();
	client->setNickname(newNick);

	// If already authenticated, notify about nick change
	if (client->isAuthenticated() && !oldNick.empty()) {
		std::string nickMsg = ":" + oldPrefix + " NICK :" + newNick + "\r\n";
		_server->sendToClient(client, nickMsg);
		_server->broadcastToClientChannels(client, nickMsg, client);
	}

	// Check if registration is complete
	if (client->hasPassword() && client->hasNick() && client->hasUser() && !client->isAuthenticated()) {
		client->setAuthenticated(true);
		std::string welcome = createReply(RPL::WELCOME, client->getNickname(),
			":Welcome to the Internet Relay Network " + client->getPrefix());
		_server->sendToClient(client, welcome);
	}
}

void CommandHandler::handleUser(Client* client, const Message& msg) {
	// Check if already registered
	if (client->isAuthenticated()) {
		_server->sendToClient(client, createReply(ERR::ALREADYREGISTRED, client->getNickname(), ":You may not reregister"));
		return;
	}

	// Check parameters: USER <username> <hostname> <servername> <realname>
	const std::vector<std::string>& params = msg.getParams();
	if (params.size() < 3 || msg.getTrailing().empty()) {
		_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, "*", "USER :Not enough parameters"));
		return;
	}

	// Set user information
	client->setUsername(params[0]);
	client->setRealname(msg.getTrailing());

	// Check if registration is complete
	if (client->hasPassword() && client->hasNick() && client->hasUser()) {
		client->setAuthenticated(true);
		std::string welcome = createReply(RPL::WELCOME, client->getNickname(),
			":Welcome to the Internet Relay Network " + client->getPrefix());
		_server->sendToClient(client, welcome);
	}
}

