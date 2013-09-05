#ifndef COMMAND_HANDLER_H_SEEN
#define COMMAND_HANDLER_H_SEEN

#include <string>
#include "../ipc_shared.h"

class Client;

class CommandHandler {
public:
	struct cmdHandlerFunc {
		//const char* name;
		IPC_COMMAND_CODE code;
		void (*hndFunc)(Client& client, std::string& arg);
	};

	static const cmdHandlerFunc HANDLERS[];

	static bool extractAndRun(Client& client, std::string& buf);

private:
	CommandHandler();
	CommandHandler(const CommandHandler& o);
	void operator=(const CommandHandler& o);

	static void hnd_test(Client& client, std::string& arg);
	static void hnd_getTemperature(Client& client, std::string& arg);
};

#endif /* ! COMMAND_HANDLER_H_SEEN */
