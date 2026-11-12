#include "Message.hpp"
#include <sstream>

Message::Message() : _hasTrailing(false) {
}

Message::Message(const std::string& raw) : _hasTrailing(false) {
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

bool Message::hasTrailing() const {
	return _hasTrailing;
}

// Parse IRC message format: [:<prefix>] <command> <params> [:<trailing>]
// The trailing parameter is also appended to _params as the last parameter
// (RFC 1459/2812: the leading colon is only needed when the final parameter
// contains spaces or starts with a colon), so commands can read every
// argument by position whether or not the client used a colon.
bool Message::parse(const std::string& raw) {
	if (raw.empty())
		return false;

	_prefix.clear();
	_command.clear();
	_params.clear();
	_trailing.clear();
	_hasTrailing = false;

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

	// Skip spaces
	while (!line.empty() && line[0] == ' ')
		line.erase(0, 1);

	// Parse command
	pos = line.find(' ');
	if (pos == std::string::npos) {
		_command = line;
		return true;
	}
	_command = line.substr(0, pos);
	line = line.substr(pos + 1);

	// Skip spaces
	while (!line.empty() && line[0] == ' ')
		line.erase(0, 1);

	// Parse parameters and trailing
	while (!line.empty()) {
		if (line[0] == ':') {
			_trailing = line.substr(1);
			_hasTrailing = true;
			_params.push_back(_trailing);
			break;
		}
		pos = line.find(' ');
		if (pos == std::string::npos) {
			_params.push_back(line);
			break;
		}
		_params.push_back(line.substr(0, pos));
		line = line.substr(pos + 1);
		
		// Skip multiple spaces
		while (!line.empty() && line[0] == ' ')
			line.erase(0, 1);
	}

	return true;
}

// Utility
std::string Message::toString() const {
	std::ostringstream oss;

	if (!_prefix.empty())
		oss << ":" << _prefix << " ";

	oss << _command;

	for (size_t i = 0; i < _params.size(); ++i) {
		if (_hasTrailing && i + 1 == _params.size())
			oss << " :" << _params[i];
		else
			oss << " " << _params[i];
	}

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

std::string ircToLower(const std::string& str) {
	std::string result(str);
	for (size_t i = 0; i < result.length(); ++i) {
		char c = result[i];
		if (c >= 'A' && c <= 'Z')
			result[i] = static_cast<char>(c - 'A' + 'a');
		else if (c == '[')
			result[i] = '{';
		else if (c == ']')
			result[i] = '}';
		else if (c == '\\')
			result[i] = '|';
		else if (c == '~')
			result[i] = '^';
	}
	return result;
}
