#ifndef MESSAGE_HPP
# define MESSAGE_HPP

# include <string>
# include <vector>

class Message {
private:
	std::string					_prefix;
	std::string					_command;
	std::vector<std::string>	_params;
	std::string					_trailing;

public:
	// Constructor & Destructor
	Message();
	Message(const std::string& raw);
	~Message();

	// Getters
	const std::string&				getPrefix() const;
	const std::string&				getCommand() const;
	const std::vector<std::string>&	getParams() const;
	const std::string&				getTrailing() const;

	// Parse
	bool							parse(const std::string& raw);

	// Utility
	std::string						toString() const;
};

// IRC numeric replies
namespace RPL {
	const int WELCOME = 1;
	const int NOTOPIC = 331;
	const int TOPIC = 332;
	const int NAMREPLY = 353;
	const int ENDOFNAMES = 366;
}

// IRC error replies
namespace ERR {
	const int NOSUCHNICK = 401;
	const int NOSUCHCHANNEL = 403;
	const int CANNOTSENDTOCHAN = 404;
	const int UNKNOWNCOMMAND = 421;
	const int NORECIPIENT = 411;
	const int NOTEXTTOSEND = 412;
	const int NONICKNAMEGIVEN = 431;
	const int ERRONEUSNICKNAME = 432;
	const int NICKNAMEINUSE = 433;
	const int USERNOTINCHANNEL = 441;
	const int NOTONCHANNEL = 442;
	const int USERONCHANNEL = 443;
	const int NOTREGISTERED = 451;
	const int NEEDMOREPARAMS = 461;
	const int ALREADYREGISTRED = 462;
	const int PASSWDMISMATCH = 464;
	const int CHANNELISFULL = 471;
	const int UNKNOWNMODE = 472;
	const int INVITEONLYCHAN = 473;
	const int BADCHANNELKEY = 475;
	const int CHANOPRIVSNEEDED = 482;
}

// Helper functions for creating IRC messages
std::string createReply(int code, const std::string& client, const std::string& msg);
std::string createReply(int code, const std::string& client, const std::string& target, const std::string& msg);

#endif

