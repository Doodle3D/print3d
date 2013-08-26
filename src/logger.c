#include <stdarg.h>
#include <time.h>
#include "print3d.h"

static ELOG_LEVEL log_level = BULK;
static FILE* log_stream = NULL;

int log_open_stream(FILE* stream, ELOG_LEVEL level) {
	int rv = 0;
	//TODO: return error if not writable
	if ((rv = log_close()) >= 0) {
		log_stream = stream;
		log_set_level(level);
	}

	return rv;
}

int log_open_file(const char* file, ELOG_LEVEL level) {
	int rv = 0;

	if ((rv = log_close()) >= 0) {
		//TODO: open filename (how to handle existing files?)
		log_set_level(level);
	}

	return rv;
}

int log_close() {
	int rv = 0;

	if (log_stream != NULL) rv = fclose(log_stream);

	return rv;
}

bool log_is_open() {
	return (log_stream != NULL);
}

void log_set_level(ELOG_LEVEL level) {
	log_level = level;
}

ELOG_LEVEL log_get_level() {
	return log_level;
}

void log_msg(ELOG_LEVEL level, const char* format, ...) {
	va_list args;
	time_t ltime;
	struct tm* now;

	if (!log_stream || level > log_level) return;

	ltime = time(NULL);
	now = localtime(&ltime);
	fprintf(log_stream, "%02i-%02i %02i:%02i:%02i  ",now->tm_mday, now->tm_mon + 1, now->tm_hour, now->tm_min, now->tm_sec);

	va_start(args, format);
	vfprintf(log_stream, format, args);
	va_end(args);
}
