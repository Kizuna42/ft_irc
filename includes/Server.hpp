#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include <set>
#include <poll.h>

class Client;
class Channel;
class CommandHandler;

class Server {
private:
    int _serverFd;
    int _port;
    std::string _password;
    std::vector<struct pollfd> _pollFds;
    std::map<int, Client*> _clients;
    std::map<std::string, Channel*> _channels;
    CommandHandler* _commandHandler;
    bool _running;

public:
    Server(int port, const std::string& password);
    ~Server();

    // Server operations
    void start();
    void stop();
    bool isRunning() const;

    // Client management
    Client* getClient(int fd);
    Client* getClientByNickname(const std::string& nickname);
    void removeClient(int fd, const std::string& reason = "");
    void disconnectClient(Client* client, const std::string& reason);
    void sendToClient(Client* client, const std::string& message);

    // Channel management
    Channel* getChannel(const std::string& name);
    Channel* createChannel(const std::string& name);
    void removeChannel(const std::string& name);
    void broadcastToClientChannels(Client* client, const std::string& message, Client* exclude);

    // Misc
    const std::string& getPassword() const;

private:
    // Internal helpers
    void setupSocket();
    void handleNewConnection();
    void handleClientData(int fd);
    void handleClientSend(int fd);
    void closeConnection(int fd, const std::string& reason = "");
    void setNonBlocking(int fd);
    void processClientMessage(Client* client, const std::string& message);
};

#endif
