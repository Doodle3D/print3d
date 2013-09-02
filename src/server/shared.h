#ifdef __cplusplus
extern "C" {
#endif

typedef enum COMMAND_INDICES {
	CMD_TEST_IDX, CMD_GET_TEMPERATURE_IDX
} COMMAND_INDICES;

extern const char* COMMAND_NAMES[];

extern const char* SOCKET_PATH_PREFIX;

#ifdef __cplusplus
} //extern "C"
#endif
