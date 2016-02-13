/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "CommandHandler.h"
#include "Client.h"
#include "Server.h"
#include "../utils.h"

using std::string;

//NOTE: see Server.cpp for comments on these macros
#define LOG(lvl, fmt, ...) Logger::getInstance().log(lvl, "CMDH", fmt, ##__VA_ARGS__)


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

const Logger::ELOG_LEVEL CommandHandler::COMMAND_LOG_LEVEL = Logger::BULK;

//static
//expects a complete command at start of buffer
void CommandHandler::runCommand(Client& client, const char* buf, int buflen) {
	const handlerFunctions* hfunc = HANDLERS;
	IPC_COMMAND_CODE code = ipc_cmd_get(buf, buflen);

	Logger::getInstance().logIpcCmd(Logger::BULK, buf, buflen);

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
	LOG(COMMAND_LOG_LEVEL, "test cmd with %i arguments", numargs);

	char* argtext = 0;
	if (numargs > 0) {
		char* arg = 0;
		ipc_cmd_get_string_arg(buf, buflen, 0, &arg);
		asprintf(&argtext, "printserver test answer to the question: '%s'", arg);
		free(arg);
	} else {
		asprintf(&argtext, "printserver test answer without question");
	}

	int cmdlen;
	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, "x", argtext, strlen(argtext));
	free(argtext);

	client.sendData(cmd, cmdlen);
	free(cmd);
}

//static
void CommandHandler::hnd_getTemperature(Client& client, const char* buf, int buflen) {
	if (ipc_cmd_num_args(buf, buflen) > 0) {
		AbstractDriver* driver = client.getServer().getDriver();

		int16_t arg;
		ipc_cmd_get_short_arg(buf, buflen, 0, &arg);
		IPC_TEMPERATURE_PARAMETER which = (IPC_TEMPERATURE_PARAMETER)arg;
		LOG(COMMAND_LOG_LEVEL, "get temperature cmd with arg %i", which);
		int temp = 0;
		switch(which) {
		case IPC_TEMP_HOTEND: temp = driver->getTemperature(); break;
		case IPC_TEMP_HOTEND_TGT: temp = driver->getTargetTemperature(); break;
		case IPC_TEMP_BED: temp = driver->getBedTemperature(); break;
		case IPC_TEMP_BED_TGT: temp = driver->getTargetBedTemperature(); break;
		case IPC_TEMP_HEATING: temp = driver->isHeating() ? 1 : 0; break;
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
			LOG(Logger::ERROR, "get temperature cmd with invalid parameter value");
			client.sendError("unknown temperature parameter value");
		}
	} else {
		LOG(Logger::ERROR, "get temperature cmd without argument");
		client.sendError("missing argument");
	}
}

//static
void CommandHandler::hnd_gcodeClear(Client& client, const char* buf, int buflen) {
	LOG(COMMAND_LOG_LEVEL, "clear gcode cmd");
	Server &server = client.getServer();
	server.cancelAllTransactions(&client);
	server.getDriver()->clearGCode();
	client.sendOk();
}

//static
void CommandHandler::hnd_gcodeAppend(Client& client, const char* buf, int buflen) {
	Server &server = client.getServer();
	Client::Transaction &transaction = client.getTransaction();

	int numArgs = ipc_cmd_num_args(buf, buflen);
	if (numArgs == 0) {
		LOG(Logger::ERROR, "received append gcode cmd without argument");
		client.sendError("missing argument");
		return;
	}

	if (transaction.cancelled) {
		string msg = "transaction cancelled";
		client.sendReply(IPC_CMDR_TRX_CANCELLED, &msg);
	}

	int16_t transactionFlags = TRX_FIRST_CHUNK_BIT | TRX_LAST_CHUNK_BIT; //default to treating each chunk as a separate transaction
	if (numArgs >= 2) {
		ipc_cmd_get_short_arg(buf, buflen, 1, &transactionFlags);
	}

	if (transactionFlags & TRX_FIRST_CHUNK_BIT) {
		LOG(COMMAND_LOG_LEVEL, "hnd_gcodeAppend(): clearing gcode transaction buffer");
		transaction.buffer.clear();
		transaction.active = true;
	}

	int32_t totalLines = -1;
	GCodeBuffer::MetaData metaData;

	if (numArgs >= 3) ipc_cmd_get_long_arg(buf, buflen, 2, &totalLines);
	if (numArgs >= 4) ipc_cmd_get_long_arg(buf, buflen, 3, &metaData.seqNumber);
	if (numArgs >= 5) ipc_cmd_get_long_arg(buf, buflen, 4, &metaData.seqTotal);
	if (numArgs >= 6) {
		char *srcArg = 0;
		ipc_cmd_get_string_arg(buf, buflen, 5, &srcArg);
		metaData.source = new string(srcArg);
		free(srcArg);
	}

	char* data = 0;
	ipc_cmd_get_string_arg(buf, buflen, 0, &data);
	LOG(COMMAND_LOG_LEVEL, "hnd_gcodeAppend(): append gcode cmd with arg length %i (%i args) [ttl_lines: %i, seq_num %i, seq_ttl: %i, src: %s]", strlen(data),
			numArgs, totalLines, metaData.seqNumber, metaData.seqTotal, metaData.source ? metaData.source->c_str() : "(null)");
	transaction.buffer.append(data);
	free(data);

	if (transactionFlags & TRX_LAST_CHUNK_BIT) {
		LOG(COMMAND_LOG_LEVEL, "hnd_gcodeAppend(): appending and clearing gcode transaction buffer");
		AbstractDriver* driver = server.getDriver();
		GCodeBuffer::GCODE_SET_RESULT gsr = driver->appendGCode(transaction.buffer, totalLines, &metaData);
		transaction.buffer.clear();
		transaction.active = false;
		if (gsr != GCodeBuffer::GSR_OK) {
			if (metaData.source) delete metaData.source;
			client.sendReply(IPC_CMDR_GCODE_ADD_FAILED, &GCodeBuffer::getGcodeSetResultString(gsr));
			return;
		}
	}
	if (metaData.source) delete metaData.source;
	client.sendOk();
}

//static
//TODO: check that given file path is absolute
void CommandHandler::hnd_gcodeAppendFile(Client& client, const char* buf, int buflen) {
	if (ipc_cmd_num_args(buf, buflen) == 0) {
		LOG(Logger::ERROR, "append gcode file cmd without argument");
		client.sendError("missing argument");
	}

	if (client.getTransaction().active) {
		string msg = "transaction in progress";
		client.sendReply(IPC_CMDR_RETRY_LATER, &msg);
		return;
	}

	char* filename = 0;
	ipc_cmd_get_string_arg(buf, buflen, 0, &filename);
	LOG(COMMAND_LOG_LEVEL, "append gcode from file cmd with filename '%s'", filename);

	int filesize;
	char *data = readFileContents(filename, &filesize);
	if (!Logger::getInstance().checkError(data ? 0 : -1, "CMDH", "could not read contents of file '%s'", filename)) {
		LOG(COMMAND_LOG_LEVEL, "  read %i bytes of gcode", strlen(data));
		//LOG(Logger::BULK, "read gcode: '%s'", data);
		string s(data);
		free(data);
		client.getServer().getDriver()->appendGCode(s);
		client.sendOk();
	} else {
		client.sendError(errno > 0 ? strerror(errno) : "error reading file");
	}
	free(filename);
}

//static
void CommandHandler::hnd_gcodeStartPrint(Client& client, const char* buf, int buflen) {
	LOG(COMMAND_LOG_LEVEL, "start print gcode cmd");
	AbstractDriver* driver = client.getServer().getDriver();
	driver->startPrint();
	client.sendOk();
}

//static
void CommandHandler::hnd_gcodeStopPrint(Client& client, const char* buf, int buflen) {
	LOG(COMMAND_LOG_LEVEL, "stop print gcode cmd");
	AbstractDriver* driver = client.getServer().getDriver();

	//make sure no other gcode transfers continue after sending stop gcode
	client.getServer().cancelAllTransactions(&client);

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
		LOG(COMMAND_LOG_LEVEL, "heatup cmd with temperature %i", temperature);
		AbstractDriver* driver = client.getServer().getDriver();
		driver->heatup(temperature);
		client.sendOk();
	} else {
		LOG(Logger::ERROR, "heatup cmd without argument");
		client.sendError("missing argument");
	}
}

//static
void CommandHandler::hnd_getProgress(Client& client, const char* buf, int buflen) {
	LOG(COMMAND_LOG_LEVEL, "get progress cmd");
	AbstractDriver* driver = client.getServer().getDriver();

	int32_t currentLine = driver->getCurrentLine();
	int32_t bufferedLines = driver->getBufferedLines();
	int32_t totalLines = driver->getTotalLines();
	int32_t bufferSize = driver->getBufferSize();
	int32_t maxBufferSize = driver->getMaxBufferSize();

	int cmdlen;
	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, "WWWWW",
			currentLine, bufferedLines, totalLines, bufferSize, maxBufferSize);
	client.sendData(cmd, cmdlen);
	free(cmd);
}

//static
void CommandHandler::hnd_getState(Client& client, const char* buf, int buflen) {
	LOG(COMMAND_LOG_LEVEL, "get state cmd");
	AbstractDriver* driver = client.getServer().getDriver();

	const string& state = AbstractDriver::getStateString(driver->getState());

	int cmdlen;
	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, "x", state.c_str(), state.length());
	client.sendData(cmd, cmdlen);
	free(cmd);
}
