#include "shared.h"

//NOTE: this construct is here to provide plain C access to command names for frontends/communicator.c
#ifdef __cplusplus
extern "C" {
#endif

const char* COMMAND_NAMES[] = {
		"test", "getTemperature"
};

const char* SOCKET_PATH_PREFIX = "/tmp/print3d-";

#ifdef __cplusplus
} //extern "C"
#endif
