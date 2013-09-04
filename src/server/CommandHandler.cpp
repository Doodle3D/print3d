#include <stdio.h>
#include "CommandHandler.h"
#include "Client.h"
#include "Server.h"
#include "Logger.h"//TEMP
#include "../ipc_shared.h"

using std::string;

//private static
const CommandHandler::cmdHandlerFunc CommandHandler::HANDLERS[] = {
		{ COMMAND_NAMES[CMD_TEST_IDX], &CommandHandler::hnd_test },
		{ COMMAND_NAMES[CMD_GET_TEMPERATURE_IDX], &CommandHandler::hnd_getTemperature },
		{ "", 0 } /* sentinel */
};

//static
//returns true if a command could be extracted
//commands are of either on of these forms: "name;" or "name:arglen:arg;"
//whitespace (space,tab,\n,\r) in between commands is skipped
bool CommandHandler::extractAndRun(Client& client, string& buf) {
	const cmdHandlerFunc* hfunc = HANDLERS;

	int skipIdx = buf.find_first_not_of(" \t\n\r");

	if (skipIdx == string::npos) {
		buf.clear();
		return false;
	}	else if (skipIdx > 0) {
		buf = buf.substr(skipIdx);
	}

	while(hfunc->hndFunc) {
		int cmdLen = strlen(hfunc->name);
		if (strnstr(buf.c_str(), hfunc->name, buf.length()) == buf.c_str()) {
			string arg;
			int suffixLen = -1; //-1 is used to indicate incomplete/invalid command syntax below

			switch (buf.at(cmdLen)) {
			case ';':
				suffixLen = 1;
				break;
			case ':':
				//TODO
				int lenTerm = buf.find(':', cmdLen + 1);
				//only if found (lenTerm != string::npos), return false otherwise: string& lenText = buf.substr(cmdLen + 2, lenTerm - 1);
				//ensure ';' exists after argument
				//extract argument
				break;
			}

			//complete command found? then remove it from the buffer and handle it
			if (suffixLen >= 0) {
				buf = buf.substr(cmdLen + suffixLen);
				hfunc->hndFunc(client, arg);
				return true;
			}
		}

		hfunc++;
	}

	//  if no printer object yet, or type config changed, create printer object
	//  dispatch request to printer object (aka handleRequest() --> temp dummy for testing?)

	return false;
}


//static
void CommandHandler::hnd_test(Client& client, std::string& arg) {
	char* buf = 0;
	asprintf(&buf, "printserver test echo. argument: '%s'\n", arg.c_str());
	client.sendData(buf);
}

//static
//FIXME: check for null-pointer and return error if so (part of more elaborate IPC protocol)
void CommandHandler::hnd_getTemperature(Client& client, std::string& arg) {
	AbstractDriver* driver = client.getServer().getDriver();
	int temp = driver->getTemperature();

	char* buf = 0;
	asprintf(&buf, "%i", temp);
	client.sendData(buf);
}
