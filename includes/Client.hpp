#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>

class Client {
private:
	int					_fd;
	std::string			_nickname;
	std::string			_username;
	std::string			_hostname;
	std::string			_realname;
	std::string			_recvBuffer;
	std::string			_sendBuffer;
	bool				_isAuthenticated;
	bool				_hasPassword;
	bool				_hasNick;
	bool				_hasUser;

public:
	// Constructor & Destructor
	Client(int fd);
	~Client();

	// Getters
	int					getFd() const;
	const std::string&	getNickname() const;
	const std::string&	getUsername() const;
	const std::string&	getHostname() const;
	const std::string&	getRealname() const;
	const std::string&	getRecvBuffer() const;
	const std::string&	getSendBuffer() const;
	bool				isAuthenticated() const;
	bool				hasPassword() const;
	bool				hasNick() const;
	bool				hasUser() const;

	// Setters
	void				setNickname(const std::string& nickname);
	void				setUsername(const std::string& username);
	void				setHostname(const std::string& hostname);
	void				setRealname(const std::string& realname);
	void				setPassword(bool status);
	void				setAuthenticated(bool status);

	// Buffer management
	void				appendRecvBuffer(const std::string& data);
	void				appendSendBuffer(const std::string& data);
	void				clearRecvBuffer();
	void				clearSendBuffer(size_t len);
	bool				hasCompleteMessage() const;
	std::string			extractMessage();

	// Utility
	std::string			getPrefix() const;
};

#endif

