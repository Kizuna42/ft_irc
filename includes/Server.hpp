#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include <poll.h>
#include <cstddef>
#include <csignal>

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
    // Connections scheduled to be closed once the current poll() iteration
    // has been fully processed (fd -> reason). Closing them immediately from
    // inside a broadcast would invalidate iterators held by the caller.
    std::map<int, std::string> _pendingClose;
    static volatile sig_atomic_t s_shutdownRequested;
    static const size_t MAX_SEND_BUFFER_SIZE = 16384;
    static const size_t MAX_RECV_BUFFER_SIZE = 8192;

public:
    Server(int port, const std::string& password);
    ~Server();

    // Server operations
    void start();
    void stop();
    bool isRunning() const;
    static void requestShutdown();

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
    void renameInvites(const std::string& oldNick, const std::string& newNick);
    void broadcastToClientChannels(Client* client, const std::string& message, Client* exclude = NULL);

    // Misc
    const std::string& getPassword() const;

private:
    // Internal helpers
    void setupSocket();
    void handleNewConnection();
    void handleClientData(int fd);
    void handleClientSend(int fd);
    void closeConnection(int fd, const std::string& reason = "");
    void scheduleClose(int fd, const std::string& reason);
    bool isClosePending(int fd) const;
    void flushPendingCloses();
    void updatePollEvents();
    void setNonBlocking(int fd);
    void processClientMessage(Client* client, const std::string& message);
};

#endif
