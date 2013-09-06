#ifndef COMMAND_HANDLER_H_SEEN
#define COMMAND_HANDLER_H_SEEN

#include <string>
#include "../ipc_shared.h"

class Client;

class CommandHandler {
public:
	typedef void (*handler_func)(Client& client, const char* buf, int buflen);

	struct handlerFunctions {
		IPC_COMMAND_CODE code;
		handler_func hndFunc;
	};

	static const handlerFunctions HANDLERS[];

	static void runCommand(Client& client, const char* buf, int buflen);

private:
	CommandHandler();
	CommandHandler(const CommandHandler& o);
	void operator=(const CommandHandler& o);

	static void hnd_test(Client& client, const char* buf, int buflen);
	static void hnd_getTemperature(Client& client, const char* buf, int buflen);
};

#endif /* ! COMMAND_HANDLER_H_SEEN */
