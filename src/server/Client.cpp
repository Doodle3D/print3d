#include <sys/socket.h>
#include "Client.h"
#include "Logger.h"
#include "Server.h"
#include "CommandHandler.h"
#include "../ipc_shared.h"
#include "../utils.h"

Client::Client(Server& server, int fd)
: server_(server), fd_(fd), buffer_(0), bufferSize_(0)
{ /* empty */ }

//TODO: find a way to use recv wit extra arg in readAndAppendAvailableData()
int Client::readData() {
	return readAndAppendAvailableData(fd_, &buffer_, &bufferSize_, 0, 1);
}

void Client::runCommands() {
	while (ipc_cmd_is_complete(buffer_, bufferSize_)) {
		CommandHandler::runCommand(*this, buffer_, bufferSize_);
		ipc_cmd_remove(&buffer_, &bufferSize_);
	}
}

bool Client::sendData(const char* buf, int buflen) {
	if (fd_ == -1) return false;

	int rv = ::send(fd_, buf, buflen, 0);

	Logger::getInstance().checkError(rv, "could not send data in client with fd %i", getFileDescriptor());

	return (rv == buflen);
}


bool Client::sendOk() {
	int cmdlen;
	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, "");
	sendData(cmd, cmdlen);
	free(cmd);
}

bool Client::sendError(const std::string& message) {
	int cmdlen;
	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_ERROR, "s", message.c_str());
	sendData(cmd, cmdlen);
	free(cmd);
}


int Client::getFileDescriptor() const {
	return fd_;
}

const char* Client::getBuffer() const {
	return buffer_;
}

int Client::getBufferSize() const {
	return bufferSize_;
}

Server& Client::getServer() {
	return server_;
}

const Server& Client::getServer() const {
	return server_;
}
