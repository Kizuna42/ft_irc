#include "Message.hpp"
#include <sstream>

Message::Message() {
}

Message::Message(const std::string& raw) {
	parse(raw);
}

Message::~Message() {
}

// Getters
const std::string& Message::getPrefix() const {
	return _prefix;
}

const std::string& Message::getCommand() const {
	return _command;
}

const std::vector<std::string>& Message::getParams() const {
	return _params;
}

const std::string& Message::getTrailing() const {
	return _trailing;
}

// Parse IRC message format: [:<prefix>] <command> <params> [:<trailing>]
bool Message::parse(const std::string& raw) {
	if (raw.empty())
		return false;

	_prefix.clear();
	_command.clear();
	_params.clear();
	_trailing.clear();

	std::string line = raw;
	size_t pos = 0;

	// Parse prefix (optional)
	if (line[0] == ':') {
		pos = line.find(' ');
		if (pos == std::string::npos)
			return false;
		_prefix = line.substr(1, pos - 1);
		line = line.substr(pos + 1);
	}

	// Parse command
	pos = line.find(' ');
	if (pos == std::string::npos) {
		_command = line;
		return true;
	}
	_command = line.substr(0, pos);
	line = line.substr(pos + 1);

	// Parse parameters and trailing
	while (!line.empty()) {
		if (line[0] == ':') {
			_trailing = line.substr(1);
			break;
		}
		pos = line.find(' ');
		if (pos == std::string::npos) {
			_params.push_back(line);
			break;
		}
		_params.push_back(line.substr(0, pos));
		line = line.substr(pos + 1);
	}

	return true;
}

// Utility
std::string Message::toString() const {
	std::ostringstream oss;

	if (!_prefix.empty())
		oss << ":" << _prefix << " ";

	oss << _command;

	for (size_t i = 0; i < _params.size(); ++i)
		oss << " " << _params[i];

	if (!_trailing.empty())
		oss << " :" << _trailing;

	oss << "\r\n";
	return oss.str();
}

// Helper functions
std::string createReply(int code, const std::string& client, const std::string& msg) {
	std::ostringstream oss;
	oss << ":localhost ";
	if (code < 10)
		oss << "00" << code;
	else if (code < 100)
		oss << "0" << code;
	else
		oss << code;
	oss << " " << client << " " << msg << "\r\n";
	return oss.str();
}

std::string createReply(int code, const std::string& client, const std::string& target, const std::string& msg) {
	std::ostringstream oss;
	oss << ":localhost ";
	if (code < 10)
		oss << "00" << code;
	else if (code < 100)
		oss << "0" << code;
	else
		oss << code;
	oss << " " << client << " " << target << " " << msg << "\r\n";
	return oss.str();
}

