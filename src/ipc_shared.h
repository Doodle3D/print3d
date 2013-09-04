#ifdef __cplusplus
extern "C" {
#endif

typedef enum COMMAND_INDICES {
	CMD_TEST_IDX, CMD_GET_TEMPERATURE_IDX
} COMMAND_INDICES;

extern const char* IPC_COMMAND_NAMES[];

extern const char* IPC_SOCKET_PATH_PREFIX;

char* number_to_string(int n);
char* ipc_construct_socket_path(const char* deviceId);
char* ipc_construct_command(const char* name, const char* arg, int* len);

#ifdef __cplusplus
} //extern "C"
#endif
