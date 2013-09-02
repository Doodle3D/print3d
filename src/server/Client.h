#ifndef CLIENT_H_SEEN
#define CLIENT_H_SEEN

#include <string>

class Server;

class Client {
public:
	Client(Server& server, int fd);
	int readData();
	void runCommands();

	bool sendData(const std::string& data);

	int getFileDescriptor() const;
	Server& getServer();
	const Server& getServer() const;

private:
	static const int READ_BUFFER_LENGTH;

	Client(const Client& o);
	void operator=(const Client& o);

	Server& server_;

	int fd_;
	std::string buffer_;
};

#endif /* ! CLIENT_H_SEEN */
