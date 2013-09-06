#include <stdio.h>
#include "CommandHandler.h"
#include "Client.h"
#include "Server.h"
#include "Logger.h"//TEMP

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
//	char* argtext = 0;
//	asprintf(&argtext, "printserver test echo. argument: '%s'\n", arg.c_str());
//
//	int cmdlen;
//	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, "s", argtext);
//	free(argtext);

	//TEMP placeholder
	int cmdlen;
	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, 0);

	client.sendData(cmd, cmdlen);
}

//static
void CommandHandler::hnd_getTemperature(Client& client, const char* buf, int buflen) {
	AbstractDriver* driver = client.getServer().getDriver();
	int temp = driver->getTemperature();

	static int adder = 0;
	temp += adder++;

	int cmdlen;
	char* cmd = ipc_construct_cmd(&cmdlen, IPC_CMDR_OK, "w", temp);
	client.sendData(cmd, cmdlen);
}
