#ifndef SERVER_H_SEEN
#define SERVER_H_SEEN

#include <sys/un.h>
#include <string>
#include "Logger.h"
#include "Serial.h"

class Server {
public:
	static const bool FORK_BY_DEFAULT;

	Server(const std::string& serialPortName, const std::string& socketPath);
	~Server();

	int start(bool fork = FORK_BY_DEFAULT);

private:
	static const int SOCKET_MAX_BACKLOG;

	const std::string& serialPortName_;
	const std::string& socketPath_;

	const Logger& log_;
	int socketFd_;
	Serial serial_;

	bool openSocket();
	bool closeSocket();
	int forkProcess();
	int openPort();
};

#endif /* ! SERVER_H_SEEN */
