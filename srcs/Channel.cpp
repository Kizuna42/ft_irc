#include "Channel.hpp"
#include "Client.hpp"
#include <algorithm>

Channel::Channel(const std::string& name) : _name(name), _userLimit(0),
											 _inviteOnly(false), _topicRestricted(true) {
}

Channel::~Channel() {
}

// Getters
const std::string& Channel::getName() const {
	return _name;
}

const std::string& Channel::getTopic() const {
	return _topic;
}

const std::string& Channel::getKey() const {
	return _key;
}

size_t Channel::getUserLimit() const {
	return _userLimit;
}

bool Channel::isInviteOnly() const {
	return _inviteOnly;
}

bool Channel::isTopicRestricted() const {
	return _topicRestricted;
}

const std::vector<Client*>& Channel::getMembers() const {
	return _members;
}

// Setters
void Channel::setTopic(const std::string& topic) {
	_topic = topic;
}

void Channel::setKey(const std::string& key) {
	_key = key;
}

void Channel::setUserLimit(size_t limit) {
	_userLimit = limit;
}

void Channel::setInviteOnly(bool status) {
	_inviteOnly = status;
}

void Channel::setTopicRestricted(bool status) {
	_topicRestricted = status;
}

// Member management
void Channel::addMember(Client* client) {
	if (!isMember(client))
		_members.push_back(client);
}

void Channel::removeMember(Client* client) {
	std::vector<Client*>::iterator it = std::find(_members.begin(), _members.end(), client);
	if (it != _members.end())
		_members.erase(it);
	_operators.erase(client);
}

bool Channel::isMember(Client* client) const {
	return std::find(_members.begin(), _members.end(), client) != _members.end();
}

bool Channel::isOperator(Client* client) const {
	return _operators.find(client) != _operators.end();
}

void Channel::addOperator(Client* client) {
	if (isMember(client))
		_operators.insert(client);
}

void Channel::removeOperator(Client* client) {
	_operators.erase(client);
}

size_t Channel::getMemberCount() const {
	return _members.size();
}

// Invite management
void Channel::addInvite(const std::string& nickname) {
	_invitedUsers.insert(nickname);
}

void Channel::removeInvite(const std::string& nickname) {
	_invitedUsers.erase(nickname);
}

bool Channel::isInvited(const std::string& nickname) const {
	return _invitedUsers.find(nickname) != _invitedUsers.end();
}

// Utility
void Channel::broadcast(const std::string& message, Client* exclude) {
	for (std::vector<Client*>::iterator it = _members.begin(); it != _members.end(); ++it) {
		if (*it != exclude)
			(*it)->appendSendBuffer(message);
	}
}

std::string Channel::getMemberList() const {
	std::string list;
	for (std::vector<Client*>::const_iterator it = _members.begin(); it != _members.end(); ++it) {
		if (isOperator(*it))
			list += "@";
		list += (*it)->getNickname();
		if (it + 1 != _members.end())
			list += " ";
	}
	return list;
}

