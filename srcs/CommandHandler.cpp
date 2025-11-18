#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Message.hpp"
#include <algorithm>
#include <cctype>
#include <unistd.h>

CommandHandler::CommandHandler(Server* server) : _server(server) {
}

CommandHandler::~CommandHandler() {
}

void CommandHandler::execute(Client* client, const Message& msg) {
	std::string command = msg.getCommand();

	// Convert command to uppercase
	std::transform(command.begin(), command.end(), command.begin(), ::toupper);

	// Authentication commands (can be used before authentication)
	if (command == "PASS") {
		handlePass(client, msg);
	} else if (command == "NICK") {
		handleNick(client, msg);
	} else if (command == "USER") {
		handleUser(client, msg);
	} else if (command == "PING") {
		handlePing(client, msg);
	} else if (command == "QUIT") {
		handleQuit(client, msg);
	}
	// Commands requiring authentication
	else if (!client->isAuthenticated()) {
		_server->sendToClient(client, createReply(ERR::NOTREGISTERED, "*", ":You have not registered"));
	}
	// Channel commands
	else if (command == "JOIN") {
		handleJoin(client, msg);
	} else if (command == "PART") {
		handlePart(client, msg);
	}
	// Message commands
	else if (command == "PRIVMSG") {
		handlePrivmsg(client, msg);
	} else if (command == "NOTICE") {
		handleNotice(client, msg);
	}
	// Operator commands
	else if (command == "KICK") {
		handleKick(client, msg);
	} else if (command == "INVITE") {
		handleInvite(client, msg);
	} else if (command == "TOPIC") {
		handleTopic(client, msg);
	} else if (command == "MODE") {
		handleMode(client, msg);
	} else {
		std::string target = client->getNickname().empty() ? "*" : client->getNickname();
		_server->sendToClient(client, createReply(ERR::UNKNOWNCOMMAND, target, command + " :Unknown command"));
	}
}

void CommandHandler::handlePing(Client* client, const Message& msg) {
	std::string response = ":localhost PONG localhost";
	if (!msg.getParams().empty())
		response += " :" + msg.getParams()[0];
	else if (!msg.getTrailing().empty())
		response += " :" + msg.getTrailing();
	response += "\r\n";
	_server->sendToClient(client, response);
}

void CommandHandler::handleQuit(Client* client, const Message& msg) {
	std::string reason = msg.getTrailing().empty() ? "Client quit" : msg.getTrailing();
	_server->disconnectClient(client, reason);
}

// Helper functions
bool CommandHandler::isValidNickname(const std::string& nickname) const {
	if (nickname.empty() || nickname.length() > 9)
		return false;

	if (!std::isalpha(nickname[0]) && nickname[0] != '_')
		return false;

	for (size_t i = 1; i < nickname.length(); ++i) {
		if (!std::isalnum(nickname[i]) && nickname[i] != '_' && nickname[i] != '-')
			return false;
	}

	return true;
}

bool CommandHandler::isValidChannelName(const std::string& name) const {
	if (name.empty() || (name[0] != '#' && name[0] != '&'))
		return false;

	if (name.length() > 50)
		return false;

	for (size_t i = 1; i < name.length(); ++i) {
		if (name[i] == ' ' || name[i] == ',' || name[i] == ':')
			return false;
	}

	return true;
}

