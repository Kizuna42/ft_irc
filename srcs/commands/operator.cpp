#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Message.hpp"
#include <sstream>
#include <cstdlib>

namespace {
void appendAppliedMode(std::string& appliedModes, char& currentSign, bool adding, char modeChar)
{
	char signChar = adding ? '+' : '-';
	if (appliedModes.empty() || currentSign != signChar)
	{
		appliedModes += signChar;
		currentSign = signChar;
	}
	appliedModes += modeChar;
}
}

void CommandHandler::handleKick(Client *client, const Message &msg)
{
	// Check parameters
	const std::vector<std::string> &params = msg.getParams();
	if (params.size() < 2)
	{
		_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, client->getNickname(), "KICK :Not enough parameters"));
		return;
	}

	std::string channelName = params[0];
	std::string targetNick = params[1];
	std::string reason = msg.getTrailing().empty() ? client->getNickname() : msg.getTrailing();

	// Get channel
	Channel *channel = _server->getChannel(channelName);
	if (!channel)
	{
		_server->sendToClient(client, createReply(ERR::NOSUCHCHANNEL, client->getNickname(), channelName + " :No such channel"));
		return;
	}

	// Check if client is in channel
	if (!channel->isMember(client))
	{
		_server->sendToClient(client, createReply(ERR::NOTONCHANNEL, client->getNickname(), channelName + " :You're not on that channel"));
		return;
	}

	// Check if client is operator
	if (!channel->isOperator(client))
	{
		_server->sendToClient(client, createReply(ERR::CHANOPRIVSNEEDED, client->getNickname(), channelName + " :You're not channel operator"));
		return;
	}

	// Get target client
	Client *targetClient = _server->getClientByNickname(targetNick);
	if (!targetClient || !channel->isMember(targetClient))
	{
		_server->sendToClient(client, createReply(ERR::USERNOTINCHANNEL, client->getNickname(), targetNick + " " + channelName + " :They aren't on that channel"));
		return;
	}

	// Send KICK message to all members
	std::string kickMsg = ":" + client->getPrefix() + " KICK " + channelName + " " + targetNick + " :" + reason + "\r\n";
	channel->broadcast(kickMsg, NULL);

	// Remove from channel
	channel->removeMember(targetClient);

	// Delete channel if empty
	if (channel->getMemberCount() == 0)
		_server->removeChannel(channelName);
}

void CommandHandler::handleInvite(Client *client, const Message &msg)
{
	// Check parameters
	const std::vector<std::string> &params = msg.getParams();
	if (params.size() < 2)
	{
		_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, client->getNickname(), "INVITE :Not enough parameters"));
		return;
	}

	std::string targetNick = params[0];
	std::string channelName = params[1];

	// Get channel
	Channel *channel = _server->getChannel(channelName);
	if (!channel)
	{
		_server->sendToClient(client, createReply(ERR::NOSUCHCHANNEL, client->getNickname(), channelName + " :No such channel"));
		return;
	}

	// Check if client is in channel
	if (!channel->isMember(client))
	{
		_server->sendToClient(client, createReply(ERR::NOTONCHANNEL, client->getNickname(), channelName + " :You're not on that channel"));
		return;
	}

	// Check if client is operator
	if (!channel->isOperator(client))
	{
		_server->sendToClient(client, createReply(ERR::CHANOPRIVSNEEDED, client->getNickname(), channelName + " :You're not channel operator"));
		return;
	}

	// Get target client
	Client *targetClient = _server->getClientByNickname(targetNick);
	if (!targetClient)
	{
		_server->sendToClient(client, createReply(ERR::NOSUCHNICK, client->getNickname(), targetNick + " :No such nick/channel"));
		return;
	}

	// Check if target is already in channel
	if (channel->isMember(targetClient))
	{
		_server->sendToClient(client, createReply(ERR::USERONCHANNEL, client->getNickname(), targetNick + " " + channelName + " :is already on channel"));
		return;
	}

	// Add to invite list
	channel->addInvite(targetNick);

	// Send INVITE message to target
	std::string inviteMsg = ":" + client->getPrefix() + " INVITE " + targetNick + " " + channelName + "\r\n";
	_server->sendToClient(targetClient, inviteMsg);

	// Confirm to inviter
	std::string confirmMsg = ":localhost 341 " + client->getNickname() + " " + targetNick + " " + channelName + "\r\n";
	_server->sendToClient(client, confirmMsg);
}

void CommandHandler::handleTopic(Client *client, const Message &msg)
{
	// Check parameters
	const std::vector<std::string> &params = msg.getParams();
	if (params.empty())
	{
		_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, client->getNickname(), "TOPIC :Not enough parameters"));
		return;
	}

	std::string channelName = params[0];

	// Get channel
	Channel *channel = _server->getChannel(channelName);
	if (!channel)
	{
		_server->sendToClient(client, createReply(ERR::NOSUCHCHANNEL, client->getNickname(), channelName + " :No such channel"));
		return;
	}

	// Check if client is in channel
	if (!channel->isMember(client))
	{
		_server->sendToClient(client, createReply(ERR::NOTONCHANNEL, client->getNickname(), channelName + " :You're not on that channel"));
		return;
	}

	// If no topic parameter, return current topic
	if (params.size() == 1 && msg.getTrailing().empty())
	{
		if (channel->getTopic().empty())
		{
			_server->sendToClient(client, createReply(RPL::NOTOPIC, client->getNickname(), channelName + " :No topic is set"));
		}
		else
		{
			_server->sendToClient(client, createReply(RPL::TOPIC, client->getNickname(), channelName + " :" + channel->getTopic()));
		}
		return;
	}

	// Setting topic - check if topic is restricted
	if (channel->isTopicRestricted() && !channel->isOperator(client))
	{
		_server->sendToClient(client, createReply(ERR::CHANOPRIVSNEEDED, client->getNickname(), channelName + " :You're not channel operator"));
		return;
	}

	// Set new topic
	std::string newTopic = msg.getTrailing();
	channel->setTopic(newTopic);

	// Broadcast topic change
	std::string topicMsg = ":" + client->getPrefix() + " TOPIC " + channelName + " :" + newTopic + "\r\n";
	channel->broadcast(topicMsg, NULL);
}

void CommandHandler::handleMode(Client *client, const Message &msg)
{
	// Check parameters
	const std::vector<std::string> &params = msg.getParams();
	if (params.empty())
	{
		_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, client->getNickname(), "MODE :Not enough parameters"));
		return;
	}

	std::string channelName = params[0];

	// Get channel
	Channel *channel = _server->getChannel(channelName);
	if (!channel)
	{
		_server->sendToClient(client, createReply(ERR::NOSUCHCHANNEL, client->getNickname(), channelName + " :No such channel"));
		return;
	}

	// Check if client is in channel
	if (!channel->isMember(client))
	{
		_server->sendToClient(client, createReply(ERR::NOTONCHANNEL, client->getNickname(), channelName + " :You're not on that channel"));
		return;
	}

	// If no mode parameter, return current modes
	if (params.size() == 1)
	{
		std::string modes = "+";
		std::vector<std::string> modeParams;
		if (channel->isInviteOnly())
			modes += "i";
		if (channel->isTopicRestricted())
			modes += "t";
		if (!channel->getKey().empty())
		{
			modes += "k";
			modeParams.push_back(channel->getKey());
		}
		if (channel->getUserLimit() > 0)
		{
			modes += "l";
			std::ostringstream oss;
			oss << channel->getUserLimit();
			modeParams.push_back(oss.str());
		}

		std::ostringstream response;
		response << ":localhost 324 " << client->getNickname() << " " << channelName << " " << modes;
		for (size_t i = 0; i < modeParams.size(); ++i)
			response << " " << modeParams[i];
		response << "\r\n";
		_server->sendToClient(client, response.str());
		return;
	}

	// Setting modes - check if client is operator
	if (!channel->isOperator(client))
	{
		_server->sendToClient(client, createReply(ERR::CHANOPRIVSNEEDED, client->getNickname(), channelName + " :You're not channel operator"));
		return;
	}

	std::string modeStr = params[1];
	bool adding = true;
	size_t paramIndex = 2;
	char currentSign = '\0';
	std::string appliedModes;
	std::vector<std::string> appliedParams;
	bool modeChanged = false;

	for (size_t i = 0; i < modeStr.length(); ++i)
	{
		char mode = modeStr[i];

		if (mode == '+')
		{
			adding = true;
			continue;
		}
		if (mode == '-')
		{
			adding = false;
			continue;
		}

		if (mode == 'i')
		{
			channel->setInviteOnly(adding);
			modeChanged = true;
			appendAppliedMode(appliedModes, currentSign, adding, mode);
		}
		else if (mode == 't')
		{
			channel->setTopicRestricted(adding);
			modeChanged = true;
			appendAppliedMode(appliedModes, currentSign, adding, mode);
		}
		else if (mode == 'k')
		{
			if (adding)
			{
				if (paramIndex >= params.size())
				{
					_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, client->getNickname(), "MODE :Not enough parameters"));
					return;
				}
				channel->setKey(params[paramIndex]);
				modeChanged = true;
				appendAppliedMode(appliedModes, currentSign, adding, mode);
				appliedParams.push_back(params[paramIndex]);
				paramIndex++;
			}
			else
			{
				channel->setKey("");
				modeChanged = true;
				appendAppliedMode(appliedModes, currentSign, adding, mode);
			}
		}
		else if (mode == 'o')
		{
			if (paramIndex >= params.size())
			{
				_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, client->getNickname(), "MODE :Not enough parameters"));
				return;
			}
			Client *targetClient = _server->getClientByNickname(params[paramIndex]);
			if (!targetClient)
			{
				_server->sendToClient(client, createReply(ERR::NOSUCHNICK, client->getNickname(), params[paramIndex] + " :No such nick/channel"));
				paramIndex++;
				continue;
			}
			if (!channel->isMember(targetClient))
			{
				_server->sendToClient(client, createReply(ERR::USERNOTINCHANNEL, client->getNickname(), params[paramIndex] + " " + channelName + " :They aren't on that channel"));
				paramIndex++;
				continue;
			}
			if (adding)
				channel->addOperator(targetClient);
			else
				channel->removeOperator(targetClient);
			modeChanged = true;
			appendAppliedMode(appliedModes, currentSign, adding, mode);
			appliedParams.push_back(params[paramIndex]);
			paramIndex++;
		}
		else if (mode == 'l')
		{
			if (adding)
			{
				if (paramIndex >= params.size())
				{
					_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, client->getNickname(), "MODE :Not enough parameters"));
					return;
				}
				int limit = std::atoi(params[paramIndex].c_str());
				if (limit <= 0)
				{
					_server->sendToClient(client, createReply(ERR::NEEDMOREPARAMS, client->getNickname(), "MODE :Limit must be positive"));
					paramIndex++;
					continue;
				}
				channel->setUserLimit(limit);
				modeChanged = true;
				appendAppliedMode(appliedModes, currentSign, adding, mode);
				appliedParams.push_back(params[paramIndex]);
				paramIndex++;
			}
			else
			{
				channel->setUserLimit(0);
				modeChanged = true;
				appendAppliedMode(appliedModes, currentSign, adding, mode);
			}
		}
		else
		{
			_server->sendToClient(client, createReply(ERR::UNKNOWNMODE, client->getNickname(), std::string(1, mode) + " :is unknown mode char to me"));
		}
	}

	// Broadcast mode change
	if (modeChanged && appliedModes.length() > 1)
	{
		std::ostringstream msg;
		msg << ":" << client->getPrefix() << " MODE " << channelName << " " << appliedModes;
		for (size_t i = 0; i < appliedParams.size(); ++i)
			msg << " " << appliedParams[i];
		msg << "\r\n";
		channel->broadcast(msg.str(), NULL);
	}
}
