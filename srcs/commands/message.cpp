#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Message.hpp"

void CommandHandler::handlePrivmsg(Client* client, const Message& msg) {
	// Check parameters
	const std::vector<std::string>& params = msg.getParams();
	if (params.empty()) {
		_server->sendToClient(client, createReply(ERR::NORECIPIENT, client->getNickname(), ":No recipient given (PRIVMSG)"));
		return;
	}

	if (msg.getTrailing().empty()) {
		_server->sendToClient(client, createReply(ERR::NOTEXTTOSEND, client->getNickname(), ":No text to send"));
		return;
	}

	std::string target = params[0];
	std::string text = msg.getTrailing();
	std::string privmsgMsg = ":" + client->getPrefix() + " PRIVMSG " + target + " :" + text + "\r\n";

	// Check if target is a channel
	if (target[0] == '#' || target[0] == '&') {
		Channel* channel = _server->getChannel(target);
		if (!channel) {
			_server->sendToClient(client, createReply(ERR::NOSUCHCHANNEL, client->getNickname(), target + " :No such channel"));
			return;
		}

		if (!channel->isMember(client)) {
			_server->sendToClient(client, createReply(ERR::CANNOTSENDTOCHAN, client->getNickname(), target + " :Cannot send to channel"));
			return;
		}

		// Broadcast to all members except sender
		channel->broadcast(privmsgMsg, client);
	}
	// Target is a user
	else {
		Client* targetClient = _server->getClientByNickname(target);
		if (!targetClient) {
			_server->sendToClient(client, createReply(ERR::NOSUCHNICK, client->getNickname(), target + " :No such nick/channel"));
			return;
		}

		_server->sendToClient(targetClient, privmsgMsg);
	}
}

void CommandHandler::handleNotice(Client* client, const Message& msg) {
	// NOTICE is like PRIVMSG but doesn't send error replies
	const std::vector<std::string>& params = msg.getParams();
	if (params.empty() || msg.getTrailing().empty())
		return;

	std::string target = params[0];
	std::string text = msg.getTrailing();
	std::string noticeMsg = ":" + client->getPrefix() + " NOTICE " + target + " :" + text + "\r\n";

	// Check if target is a channel
	if (target[0] == '#' || target[0] == '&') {
		Channel* channel = _server->getChannel(target);
		if (!channel || !channel->isMember(client))
			return;

		// Broadcast to all members except sender
		channel->broadcast(noticeMsg, client);
	}
	// Target is a user
	else {
		Client* targetClient = _server->getClientByNickname(target);
		if (!targetClient)
			return;

		_server->sendToClient(targetClient, noticeMsg);
	}
}

