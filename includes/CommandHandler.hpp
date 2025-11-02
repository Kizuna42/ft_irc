#ifndef COMMANDHANDLER_HPP
# define COMMANDHANDLER_HPP

# include <string>
# include <vector>

class Server;
class Client;
class Message;

class CommandHandler {
private:
	Server* _server;

public:
	// Constructor & Destructor
	CommandHandler(Server* server);
	~CommandHandler();

	// Command execution
	void execute(Client* client, const Message& msg);

	// Authentication commands
	void handlePass(Client* client, const Message& msg);
	void handleNick(Client* client, const Message& msg);
	void handleUser(Client* client, const Message& msg);

	// Channel commands
	void handleJoin(Client* client, const Message& msg);
	void handlePart(Client* client, const Message& msg);

	// Message commands
	void handlePrivmsg(Client* client, const Message& msg);
	void handleNotice(Client* client, const Message& msg);

	// Operator commands
	void handleKick(Client* client, const Message& msg);
	void handleInvite(Client* client, const Message& msg);
	void handleTopic(Client* client, const Message& msg);
	void handleMode(Client* client, const Message& msg);

	// Utility commands
	void handlePing(Client* client, const Message& msg);
	void handleQuit(Client* client, const Message& msg);

private:
	// Helper functions
	bool isValidNickname(const std::string& nickname) const;
	bool isValidChannelName(const std::string& name) const;
};

#endif

