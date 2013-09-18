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
const CommandHandler::handlerFunctions CommandHandler::HANDLERS[] = {
		{ IPC_CMDQ_TEST, &CommandHandler::hnd_test },
		{ IPC_CMDQ_GET_TEMPERATURE, &CommandHandler::hnd_getTemperature },
		{ IPC_CMDQ_GCODE_CLEAR, &CommandHandler::hnd_gcodeClear },
		{ IPC_CMDQ_GCODE_APPEND, &CommandHandler::hnd_gcodeAppend },
		{ IPC_CMDQ_GCODE_APPEND_FILE, &CommandHandler::hnd_gcodeAppendFile },
		{ IPC_CMDQ_GCODE_STARTPRINT, &CommandHandler::hnd_gcodeStartPrint },
		{ IPC_CMDQ_GCODE_STOPPRINT, &CommandHandler::hnd_gcodeStopPrint },
		{ IPC_CMDQ_HEATUP, &CommandHandler::hnd_heatup },
		{ IPC_CMDQ_GET_PROGRESS, &CommandHandler::hnd_getProgress },
		{ IPC_CMDQ_GET_STATE, &CommandHandler::hnd_getState },
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

	//TODO:
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
	if (ipc_cmd_num_args(buf, buflen) > 0) {
		AbstractDriver* driver = client.getServer().getDriver();

		IPC_TEMPERATURE_PARAMETER which = IPC_TEMP_NONE;
		ipc_cmd_get_short_arg(buf, buflen, 0, (short int*)&which);
		int temp = 0;
		switch(which) {
		case IPC_TEMP_HOTEND: temp = driver->getTemperature(); break;
		case IPC_TEMP_HOTEND_TGT: temp = driver->getTargetTemperature(); break;
		case IPC_TEMP_BED: temp = driver->getBedTemperature(); break;
		case IPC_TEMP_BED_TGT: temp = driver->getTargetBedTemperature(); break;
		default:
			/* incorrect value, set to known invalid value and handle below */;
			which = IPC_TEMP_NONE;
			break;
		}

		if (which != IPC_TEMP_NONE) {
			int cmdlen;
			char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, "w", temp);
			client.sendData(cmd, cmdlen);
			free(cmd);
		} else {
			Logger::getInstance().log(Logger::ERROR, "received get temperature command with invalid parameter value");
			client.sendError("unknown temperature parameter value");
		}
	} else {
		Logger::getInstance().log(Logger::ERROR, "received get temperature command without argument");
		client.sendError("missing argument");
	}
}

//static
void CommandHandler::hnd_gcodeClear(Client& client, const char* buf, int buflen) {
	Logger::getInstance().log(Logger::VERBOSE, "received clear gcode command");
	AbstractDriver* driver = client.getServer().getDriver();
	driver->clearGCode();
	client.sendOk();
}

//static
void CommandHandler::hnd_gcodeAppend(Client& client, const char* buf, int buflen) {
	if (ipc_cmd_num_args(buf, buflen) > 0) {
		char* data = 0;
		ipc_cmd_get_string_arg(buf, buflen, 0, &data);
		Logger::getInstance().log(Logger::VERBOSE, "received append gcode command with argument length %i", strlen(data));
		//Logger::getInstance().log(Logger::BULK, "received gcode was '%s'", data);
		AbstractDriver* driver = client.getServer().getDriver();
		string s(data);
		free(data);
		driver->appendGCode(s);
		client.sendOk();
	} else {
		Logger::getInstance().log(Logger::ERROR, "received append gcode command without argument");
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
			AbstractDriver* driver = client.getServer().getDriver();
			string s(data);
			free(data);
			driver->appendGCode(s);
			client.sendOk();
		} else {
			client.sendError(errno > 0 ? strerror(errno) : "error reading file");
		}
		free(filename);
	} else {
		Logger::getInstance().log(Logger::ERROR, "received append gcode file command without argument");
		client.sendError("missing argument");
	}
}

//static
void CommandHandler::hnd_gcodeStartPrint(Client& client, const char* buf, int buflen) {
	Logger::getInstance().log(Logger::VERBOSE, "received start print gcode command");
	AbstractDriver* driver = client.getServer().getDriver();
	driver->startPrint();
	client.sendOk();
}

//static
void CommandHandler::hnd_gcodeStopPrint(Client& client, const char* buf, int buflen) {
	Logger::getInstance().log(Logger::VERBOSE, "received stop print gcode command");
	AbstractDriver* driver = client.getServer().getDriver();

	if (ipc_cmd_num_args(buf, buflen) > 0) {
		char *argText = 0;
		ipc_cmd_get_string_arg(buf, buflen, 0, &argText);
		string endCode(argText);
		free(argText);
		driver->stopPrint(endCode);
	} else {
		driver->stopPrint();
	}
	client.sendOk();
}

//static
void CommandHandler::hnd_heatup(Client& client, const char* buf, int buflen) {
	if (ipc_cmd_num_args(buf, buflen) > 0) {
		int16_t temperature = 0;
		ipc_cmd_get_short_arg(buf, buflen, 0, &temperature);
		Logger::getInstance().log(Logger::VERBOSE, "received heatup command with temperature %i", temperature);
		AbstractDriver* driver = client.getServer().getDriver();
		driver->heatup(temperature);
		client.sendOk();
	} else {
		Logger::getInstance().log(Logger::ERROR, "received heatup command without argument");
		client.sendError("missing argument");
	}
}

//static
void CommandHandler::hnd_getProgress(Client& client, const char* buf, int buflen) {
	AbstractDriver* driver = client.getServer().getDriver();

	int16_t currentLine = driver->getCurrentLine();
	int16_t numLines = driver->getNumLines();

	int cmdlen;
	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, "ww", currentLine, numLines);
	client.sendData(cmd, cmdlen);
	free(cmd);
}

//static
void CommandHandler::hnd_getState(Client& client, const char* buf, int buflen) {
	//TODO: implement
}
