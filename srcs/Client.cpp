#include "Client.hpp"
#include <sstream>

Client::Client(int fd) : _fd(fd), _isAuthenticated(false),
						 _hasPassword(false), _hasNick(false), _hasUser(false) {
}

Client::~Client() {
}

// Getters
int Client::getFd() const {
	return _fd;
}

const std::string& Client::getNickname() const {
	return _nickname;
}

const std::string& Client::getUsername() const {
	return _username;
}

const std::string& Client::getHostname() const {
	return _hostname;
}

const std::string& Client::getRealname() const {
	return _realname;
}

const std::string& Client::getRecvBuffer() const {
	return _recvBuffer;
}

const std::string& Client::getSendBuffer() const {
	return _sendBuffer;
}

bool Client::isAuthenticated() const {
	return _isAuthenticated;
}

bool Client::hasPassword() const {
	return _hasPassword;
}

bool Client::hasNick() const {
	return _hasNick;
}

bool Client::hasUser() const {
	return _hasUser;
}

// Setters
void Client::setNickname(const std::string& nickname) {
	_nickname = nickname;
	_hasNick = true;
}

void Client::setUsername(const std::string& username) {
	_username = username;
}

void Client::setHostname(const std::string& hostname) {
	_hostname = hostname;
}

void Client::setRealname(const std::string& realname) {
	_realname = realname;
	_hasUser = true;
}

void Client::setPassword(bool status) {
	_hasPassword = status;
}

void Client::setAuthenticated(bool status) {
	_isAuthenticated = status;
}

// Buffer management
void Client::appendRecvBuffer(const std::string& data) {
	_recvBuffer += data;
}

void Client::appendSendBuffer(const std::string& data) {
	_sendBuffer += data;
}

void Client::clearRecvBuffer() {
	_recvBuffer.clear();
}

void Client::clearSendBuffer(size_t len) {
	_sendBuffer.erase(0, len);
}

bool Client::hasCompleteMessage() const {
	return _recvBuffer.find("\n") != std::string::npos;
}

std::string Client::extractMessage() {
	size_t pos = _recvBuffer.find("\n");
	if (pos == std::string::npos)
		return "";

	std::string message;
	if (pos > 0 && _recvBuffer[pos - 1] == '\r')
		message = _recvBuffer.substr(0, pos - 1);
	else
		message = _recvBuffer.substr(0, pos);

	_recvBuffer.erase(0, pos + 1);
	return message;
}

// Utility
std::string Client::getPrefix() const {
	std::ostringstream oss;
	oss << _nickname;
	if (!_username.empty())
		oss << "!" << _username;
	if (!_hostname.empty())
		oss << "@" << _hostname;
	return oss.str();
}
