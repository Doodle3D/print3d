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
#include "../ipc_shared.h"

class Logger;
class Server;

class Client {
public:
	typedef struct Transaction {
		Transaction() : active(false), cancelled(false) {}

		std::string buffer;
		bool active;
		bool cancelled;
	} Transaction;


	Client(Server& server, int fd);
	int readData();
	void runCommands();

	bool sendData(const char* buf, int buflen);
	bool sendOk();
	bool sendError(const std::string& message);
	bool sendReply(IPC_COMMAND_CODE code, const std::string *message = 0);

	//Note: no transaction setter is required since the reference can be manipulated directly
	Transaction &getTransaction();
	const Transaction &getTransaction() const;

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
	Transaction transaction_;
};

#endif /* ! CLIENT_H_SEEN */
