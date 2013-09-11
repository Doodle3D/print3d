#ifndef CLIENT_H_SEEN
#define CLIENT_H_SEEN

#include <string>

class Server;

class Client {
public:
	Client(Server& server, int fd);
	int readData();
	void runCommands();

	bool sendData(const char* buf, int buflen);
	bool sendOk();
	bool sendError(const std::string& message);


	int getFileDescriptor() const;
	const char* getBuffer() const;
	int getBufferSize() const;
	Server& getServer();
	const Server& getServer() const;

private:
	Client(const Client& o);
	void operator=(const Client& o);

	Server& server_;

	int fd_;
	char* buffer_;
	int bufferSize_;
};

#endif /* ! CLIENT_H_SEEN */
