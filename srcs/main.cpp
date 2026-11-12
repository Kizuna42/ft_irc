#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <csignal>

// Only async-signal-safe work is allowed here: set a flag and let the
// event loop exit normally so that destructors release every resource.
static void signalHandler(int signum) {
	(void)signum;
	Server::requestShutdown();
}

static bool parsePort(const char* str, int& port) {
	char* end = NULL;
	errno = 0;
	long value = std::strtol(str, &end, 10);
	if (errno != 0 || end == str || *end != '\0' || value <= 0 || value > 65535)
		return false;
	port = static_cast<int>(value);
	return true;
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
		return 1;
	}

	int port = 0;
	std::string password = argv[2];

	// Validate port
	if (!parsePort(argv[1], port)) {
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
	signal(SIGPIPE, SIG_IGN);

	try {
		Server server(port, password);
		server.start();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
