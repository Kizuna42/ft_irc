#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>

Server* g_server = NULL;

void signalHandler(int signum) {
	if (g_server) {
		std::cout << "\nShutting down server..." << std::endl;
		g_server->stop();
	}
	exit(signum);
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
		return 1;
	}

	int port = std::atoi(argv[1]);
	std::string password = argv[2];

	// Validate port
	if (port <= 0 || port > 65535) {
		std::cerr << "Error: Invalid port number (1-65535)" << std::endl;
		return 1;
	}

	// Validate password
	if (password.empty()) {
		std::cerr << "Error: Password cannot be empty" << std::endl;
		return 1;
	}

	// Setup signal handlers
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGQUIT, signalHandler);

	try {
		Server server(port, password);
		g_server = &server;
		server.start();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}

