#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <string>
# include <vector>
# include <set>

class Client;
class Server;

class Channel {
private:
	std::string				_name;
	Server*					_server;
	std::string				_topic;
	std::string				_key;
	size_t					_userLimit;
	bool					_inviteOnly;
	bool					_topicRestricted;
	std::vector<Client*>	_members;
	std::set<Client*>		_operators;
	std::set<std::string>	_invitedUsers;

public:
	// Constructor & Destructor
	Channel(const std::string& name, Server* server);
	~Channel();

	// Getters
	const std::string&		getName() const;
	const std::string&		getTopic() const;
	const std::string&		getKey() const;
	size_t					getUserLimit() const;
	bool					isInviteOnly() const;
	bool					isTopicRestricted() const;
	const std::vector<Client*>& getMembers() const;

	// Setters
	void					setTopic(const std::string& topic);
	void					setKey(const std::string& key);
	void					setUserLimit(size_t limit);
	void					setInviteOnly(bool status);
	void					setTopicRestricted(bool status);

	// Member management
	void					addMember(Client* client);
	void					removeMember(Client* client);
	bool					isMember(Client* client) const;
	bool					isOperator(Client* client) const;
	void					addOperator(Client* client);
	void					removeOperator(Client* client);
	size_t					getMemberCount() const;

	// Invite management
	void					addInvite(const std::string& nickname);
	void					removeInvite(const std::string& nickname);
	bool					isInvited(const std::string& nickname) const;

	// Utility
	void					broadcast(const std::string& message, Client* exclude = NULL);
	std::string				getMemberList() const;
};

#endif

