#ifndef LOGGER_H_SEEN
#define LOGGER_H_SEEN

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ELOG_LEVEL {
	LLVL_QUIET = -1,
	LLVL_ERROR = 0,
	LLVL_WARNING = 1,
	LLVL_INFO = 2,
	LLVL_VERBOSE = 3,
	LLVL_BULK = 4
} ELOG_LEVEL;

int log_open_stream(FILE* stream, ELOG_LEVEL level);
int log_open_file(const char* file, ELOG_LEVEL level);
int log_close();

int log_is_open();
ELOG_LEVEL log_get_level();
void log_set_level(ELOG_LEVEL level);

void log_message(ELOG_LEVEL level, const char* format, ...);
int log_check_error(int rv, const char* format, ...);

void log_va_message(ELOG_LEVEL level, const char* format, va_list args);
int log_va_check_error(int rv, const char* format, va_list args);

void log_ipc_cmd(ELOG_LEVEL level, const char *buf, int buflen, const char *fmt);

#ifdef __cplusplus
	} //extern "C"
#endif

#endif /* ! LOGGER_H_SEEN */
