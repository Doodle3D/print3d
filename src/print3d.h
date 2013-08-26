#ifndef PRINT3D_H_SEEN
#define PRINT3D_H_SEEN

#include <inttypes.h>
#include <stdio.h>

typedef uint8_t bool;
#define TRUE 1
#define FALSE 0


/* declarations for serial_io.c */
bool serial_send(const char* code);

/* declarations for logger.c */

typedef enum ELOG_LEVEL {
	QUIET = -1,
	ERROR = 0,
	WARNING = 1,
	INFO = 2,
	VERBOSE = 3,
	BULK = 4
} ELOG_LEVEL;

int log_open_stream(FILE* stream, ELOG_LEVEL level);
int log_open_file(const char* file, ELOG_LEVEL level);
int log_close();
bool log_is_open();
void log_set_level(ELOG_LEVEL level);
ELOG_LEVEL log_get_level();
void log_msg(int level, const char* format, ...);

#endif /* ! PRINT3D_H_SEEN */
