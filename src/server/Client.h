/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef CLIENT_H_SEEN
#define CLIENT_H_SEEN

#include <string>

class Logger;
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

	Logger& logger_;
	Server& server_;

	int fd_;
	char* buffer_;
	int bufferSize_;
};

#endif /* ! CLIENT_H_SEEN */
