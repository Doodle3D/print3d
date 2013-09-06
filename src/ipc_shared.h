/** @file ipc_shared.h
 * Functions related to IPC between server and frontends.
 *
 * This is all plain C code.
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "inttypes.h"

typedef enum IPC_COMMAND_CODE {
	IPC_CMD_TEST = 0, IPC_CMD_NONE = 1,
	IPC_CMD_GET_TEMPERATURE = 0x10
} IPC_COMMAND_CODE;

/** Structure to associate an IPC command code to a human-readable name.
 */
typedef struct ipc_cmd_name_s {
	IPC_COMMAND_CODE code; //! The code as used in IPC messages
	const char* name; //! The corresponding human-readable name
} ipc_cmd_name_s;

/** Definitions of names for available IPC commands.
 */
extern const ipc_cmd_name_s IPC_COMMANDS[];

extern const char* IPC_SOCKET_PATH_PREFIX;

/** Constructs an IPC socket path for the given deviceId on which a
 * corresponding server should listen and clients should connect to.
 *
 * \param deviceId the identifier of the printer to create a socket path for.
 * \return A newly allocated string
 */
char* ipc_construct_socket_path(const char* deviceId);

int ipc_cmd_set(char** buf, int* buflen, IPC_COMMAND_CODE code);
int ipc_cmd_add_arg(char** buf, int* buflen, const char* arg, uint32_t arglen);
int ipc_cmd_is_complete(const char* buf, int buflen);
int ipc_cmd_num_args(const char* buf, int buflen);
IPC_COMMAND_CODE ipc_cmd_get(const char* buf, int buflen);
int ipc_cmd_get_arg(const char* buf, int buflen, char** argbuf, int* argbuflen, int argidx, int addzero);

/** Removes the first complete command from the start of the buffer.
 *
 * \param buf Command buffer
 * \param buflen Command buffer length
 * \return 1 if a command has been removed, 0 on success or -1 on system error
 */
int ipc_cmd_remove(char** buf, int* buflen);

#ifdef __cplusplus
} //extern "C"
#endif
