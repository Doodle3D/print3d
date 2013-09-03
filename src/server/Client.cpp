#include <sys/socket.h>
#include "Client.h"
#include "Logger.h"
#include "Server.h"
#include "CommandHandler.h"

const int Client::READ_BUFFER_LENGTH = 256; //static

Client::Client(Server& server, int fd)
: server_(server), fd_(fd)
{ /* empty */ }

int Client::readData() {
	char rbuf[READ_BUFFER_LENGTH + 1] = "";

	int rv = ::recv(fd_, rbuf, READ_BUFFER_LENGTH, 0);

	if (rv > 0) {
		rbuf[rv] = '\0';
		buffer_ += rbuf;
	}

	return rv;
}

void Client::runCommands() {
	while(CommandHandler::extractAndRun(*this, buffer_));
}

bool Client::sendData(const std::string& data) {
	if (fd_ == -1) return false;

	int rv = ::send(fd_, data.c_str(), data.length(), 0);

	Logger::getInstance().checkError(rv, "could not send data in client with fd %i", getFileDescriptor());

	return (rv == data.length());
}

int Client::getFileDescriptor() const {
	return fd_;
}

Server& Client::getServer() {
	return server_;
}

const Server& Client::getServer() const {
	return server_;
}
