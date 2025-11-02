#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Message.hpp"
#include <iostream>
#include <sstream>

void CommandHandler::handleJoin(Client* client, const Message& msg) {
	// Check parameters
	const std::vector<std::string>& params = msg.getParams();
	if (params.empty()) {
		_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, client->getNickname(), "JOIN :Not enough parameters"));
		return;
	}

	std::string channelName = params[0];
	std::string key = params.size() > 1 ? params[1] : "";

	// Validate channel name
	if (!isValidChannelName(channelName)) {
		_server->sendToClient(client, createReply(ERR::NOSUCHCHANNEL, client->getNickname(), channelName + " :No such channel"));
		return;
	}

	// Get or create channel
	Channel* channel = _server->getChannel(channelName);
	bool isNewChannel = (channel == NULL);

	if (isNewChannel) {
		try {
			channel = _server->createChannel(channelName);
		} catch (const std::exception& e) {
			std::cerr << "Error: Failed to create channel (memory allocation failed): " << e.what() << std::endl;
			return;
		}
	}

	if (!channel)
		return;

	// Check if already in channel
	if (channel->isMember(client))
		return;

	// Check invite-only
	if (channel->isInviteOnly() && !channel->isInvited(client->getNickname())) {
		_server->sendToClient(client, createReply(ERR::INVITEONLYCHAN, client->getNickname(), channelName + " :Cannot join channel (+i)"));
		return;
	}

	// Check user limit
	if (channel->getUserLimit() > 0 && channel->getMemberCount() >= channel->getUserLimit()) {
		_server->sendToClient(client, createReply(ERR::CHANNELISFULL, client->getNickname(), channelName + " :Cannot join channel (+l)"));
		return;
	}

	// Check key
	if (!channel->getKey().empty() && channel->getKey() != key) {
		_server->sendToClient(client, createReply(ERR::BADCHANNELKEY, client->getNickname(), channelName + " :Cannot join channel (+k)"));
		return;
	}

	// Add to channel
	channel->addMember(client);

	// If new channel, make client operator
	if (isNewChannel)
		channel->addOperator(client);

	// Remove from invite list
	if (channel->isInvited(client->getNickname()))
		channel->removeInvite(client->getNickname());

	// Send JOIN message to all members including sender
	std::string joinMsg = ":" + client->getPrefix() + " JOIN " + channelName + "\r\n";
	channel->broadcast(joinMsg, NULL);

	// Send topic
	if (channel->getTopic().empty()) {
		_server->sendToClient(client, createReply(RPL::NOTOPIC, client->getNickname(), channelName + " :No topic is set"));
	} else {
		_server->sendToClient(client, createReply(RPL::TOPIC, client->getNickname(), channelName + " :" + channel->getTopic()));
	}

	// Send names list
	std::string memberList = channel->getMemberList();
	_server->sendToClient(client, createReply(RPL::NAMREPLY, client->getNickname(), "= " + channelName + " :" + memberList));
	_server->sendToClient(client, createReply(RPL::ENDOFNAMES, client->getNickname(), channelName + " :End of /NAMES list"));
}

void CommandHandler::handlePart(Client* client, const Message& msg) {
	// Check parameters
	const std::vector<std::string>& params = msg.getParams();
	if (params.empty()) {
		_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, client->getNickname(), "PART :Not enough parameters"));
		return;
	}

	std::string channelName = params[0];
	std::string reason = msg.getTrailing().empty() ? client->getNickname() : msg.getTrailing();

	// Get channel
	Channel* channel = _server->getChannel(channelName);
	if (!channel) {
		_server->sendToClient(client, createReply(ERR::NOSUCHCHANNEL, client->getNickname(), channelName + " :No such channel"));
		return;
	}

	// Check if in channel
	if (!channel->isMember(client)) {
		_server->sendToClient(client, createReply(ERR::NOTONCHANNEL, client->getNickname(), channelName + " :You're not on that channel"));
		return;
	}

	// Send PART message to all members including sender
	std::string partMsg = ":" + client->getPrefix() + " PART " + channelName + " :" + reason + "\r\n";
	channel->broadcast(partMsg, NULL);

	// Remove from channel
	channel->removeMember(client);

	// Delete channel if empty
	if (channel->getMemberCount() == 0)
		_server->removeChannel(channelName);
}

