/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef COMMAND_HANDLER_H_SEEN
#define COMMAND_HANDLER_H_SEEN

#include <string>
#include "Logger.h"
#include "../ipc_shared.h"

class Client;

class CommandHandler {
public:
	typedef void (*handler_func)(Client& client, const char* buf, int buflen);

	static void runCommand(Client& client, const char* buf, int buflen);

private:
	struct handlerFunctions {
		IPC_COMMAND_CODE code;
		handler_func hndFunc;
	};

	static const handlerFunctions HANDLERS[];
	static const Logger::ELOG_LEVEL COMMAND_LOG_LEVEL;

	CommandHandler();
	CommandHandler(const CommandHandler& o);
	void operator=(const CommandHandler& o);

	static void hnd_test(Client& client, const char* buf, int buflen);
	static void hnd_getTemperature(Client& client, const char* buf, int buflen);
	static void hnd_gcodeClear(Client& client, const char* buf, int buflen);
	static void hnd_gcodeAppend(Client& client, const char* buf, int buflen);
	static void hnd_gcodeAppendFile(Client& client, const char* buf, int buflen);
	static void hnd_gcodeStartPrint(Client& client, const char* buf, int buflen);
	static void hnd_gcodeStopPrint(Client& client, const char* buf, int buflen);
	static void hnd_heatup(Client& client, const char* buf, int buflen);
	static void hnd_getProgress(Client& client, const char* buf, int buflen);
	static void hnd_getState(Client& client, const char* buf, int buflen);
};

#endif /* ! COMMAND_HANDLER_H_SEEN */
