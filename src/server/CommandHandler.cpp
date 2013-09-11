#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "CommandHandler.h"
#include "Client.h"
#include "Server.h"
#include "Logger.h"
#include "../utils.h"

using std::string;

//private static
//const CommandHandler::cmdHandlerFunc CommandHandler::HANDLERS[] = {
//		{ IPC_COMMAND_NAMES[CMD_TEST_IDX], &CommandHandler::hnd_test },
//		{ IPC_COMMAND_NAMES[CMD_GET_TEMPERATURE_IDX], &CommandHandler::hnd_getTemperature },
//		{ "", 0 } /* sentinel */
//};
const CommandHandler::handlerFunctions CommandHandler::HANDLERS[] = {
		{ IPC_CMDQ_TEST, &CommandHandler::hnd_test },
		{ IPC_CMDQ_GET_TEMPERATURE, &CommandHandler::hnd_getTemperature },
		{ IPC_CMDQ_GCODE_CLEAR, &CommandHandler::hnd_gcodeClear },
		{ IPC_CMDQ_GCODE_APPEND, &CommandHandler::hnd_gcodeAppend },
		{ IPC_CMDQ_GCODE_APPEND_FILE, &CommandHandler::hnd_gcodeAppendFile },
		{ IPC_CMDQ_GCODE_STARTPRINT, &CommandHandler::hnd_gcodeStartPrint },
		{ IPC_CMDQ_GCODE_STOPPRINT, &CommandHandler::hnd_gcodeStopPrint },
		{ IPC_CMDS_NONE, 0 } /* sentinel */
};


//static
//expects a complete command at start of buffer
void CommandHandler::runCommand(Client& client, const char* buf, int buflen) {
	const handlerFunctions* hfunc = HANDLERS;
	IPC_COMMAND_CODE code = ipc_cmd_get(buf, buflen);

	while(hfunc->hndFunc) {
			if (hfunc->code == code) hfunc->hndFunc(client, buf, buflen);

		hfunc++;
	}

	//  if no printer object yet, or type config changed, create printer object
	//  dispatch request to printer object (aka handleRequest() --> temp dummy for testing?)
}


//static
void CommandHandler::hnd_test(Client& client, const char* buf, int buflen) {
	int numargs = ipc_cmd_num_args(buf, buflen);

	char* argtext = 0;
	if (numargs > 0) {
		char* arg = 0;
		ipc_cmd_get_string_arg(buf, buflen, 0, &arg);
		asprintf(&argtext, "printserver test answer to the question: '%s'", arg);
		free(arg);
	} else {
		asprintf(&argtext, "printserver test answer without question");
	}
	printf("argtext='%s'\n", argtext);

	int cmdlen;
	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, "s", argtext);
	free(argtext);

	client.sendData(cmd, cmdlen);
	free(cmd);
}

//static
void CommandHandler::hnd_getTemperature(Client& client, const char* buf, int buflen) {
	AbstractDriver* driver = client.getServer().getDriver();
	int temp = driver->getTemperature();

	int cmdlen;
	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, "w", temp);
	client.sendData(cmd, cmdlen);
	free(cmd);
}

//static
void CommandHandler::hnd_gcodeClear(Client& client, const char* buf, int buflen) {
	Logger::getInstance().log(Logger::VERBOSE, "received clear gcode command");
	//TODO: call driver->clearGcode()
	client.sendOk();
}

//static
void CommandHandler::hnd_gcodeAppend(Client& client, const char* buf, int buflen) {
	if (ipc_cmd_num_args(buf, buflen) > 0) {
		char* data = 0;
		ipc_cmd_get_string_arg(buf, buflen, 0, &data);
		Logger::getInstance().log(Logger::VERBOSE, "received append gcode command with argument length %i", strlen(data));
		Logger::getInstance().log(Logger::VERBOSE, "received gcode was '%s'", data);
		//TODO: call driver->appendGcode(data)
		free(data);
		client.sendOk();
	} else {
		Logger::getInstance().log(Logger::VERBOSE, "received append gcode command without argument");
		client.sendError("missing argument");
	}
}

//static
void CommandHandler::hnd_gcodeAppendFile(Client& client, const char* buf, int buflen) {
	if (ipc_cmd_num_args(buf, buflen) > 0) {
		char* filename = 0;
		ipc_cmd_get_string_arg(buf, buflen, 0, &filename);
		Logger::getInstance().log(Logger::VERBOSE, "received append gcode from file command with filename '%s'", filename);

		int filesize;
		char *data = readFileContents(filename, &filesize);
		if (!Logger::getInstance().checkError(data ? 0 : -1, "could not read contents of file '%s'", filename)) {
			Logger::getInstance().log(Logger::VERBOSE, "read %i bytes of gcode", strlen(data));
			//Logger::getInstance().log(Logger::BULK, "read gcode: '%s'", data);
			//TODO: call driver->appendGcode(data)
			client.sendOk();
		} else {
			client.sendError(errno > 0 ? strerror(errno) : "error reading file");
		}
	}
}

//static
void CommandHandler::hnd_gcodeStartPrint(Client& client, const char* buf, int buflen) {
	Logger::getInstance().log(Logger::VERBOSE, "received start print gcode command");
	//TODO: call driver->startPrintGcode()
	client.sendOk();
}

//static
void CommandHandler::hnd_gcodeStopPrint(Client& client, const char* buf, int buflen) {
	Logger::getInstance().log(Logger::VERBOSE, "received stop print gcode command");
	//TODO: call driver->stopPrintGcode()
	client.sendOk();
}
