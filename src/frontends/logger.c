#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "Logger.h"



static ELOG_LEVEL level_ = LLVL_BULK;
static FILE* stream_ = NULL;

int log_open_stream(FILE* stream, ELOG_LEVEL level) {
	if (stream_ == stream) return 0;

	int rv = log_close();
	//TODO: return error if not writable
	if (rv >= 0) {
		stream_ = stream;
		log_set_level(level);
	}

	return rv;
}

int log_open_file(const char* file, ELOG_LEVEL level) {
	int rv = log_close();

	if (rv >= 0) {
		//TODO: open filename (how to handle existing files?)
		log_set_level(level);
	}

	return rv;
}

int log_close() {
	int rv = 0;

	if (stream_ != NULL) rv = fclose(stream_);

	return rv;
}

int log_is_open() {
	return (stream_ != NULL);
}

ELOG_LEVEL log_get_level() {
	return level_;
}

void log_set_level(ELOG_LEVEL level) {
	level_ = level;
}

void log_message(ELOG_LEVEL level, const char* format, ...) {
	va_list args;
	va_start(args, format);
	log_va_message(level, format, args);
	va_end(args);
}

int log_check_error(int rv, const char* format, ...) {
	va_list args;
	va_start(args, format);
	int result = log_va_check_error(rv, format, args);
	va_end(args);
	return result;
}



void log_va_message(ELOG_LEVEL level, const char* format, va_list args) {
	time_t ltime;
	struct tm* now;

	if (!stream_ || level > level_) return;

	char* buf = 0;
	vasprintf(&buf, format, args);

	ltime = time(NULL);
	now = localtime(&ltime);
	fprintf(stream_, "%02i-%02i %02i:%02i:%02i  %s\n",
			now->tm_mday, now->tm_mon + 1, now->tm_hour, now->tm_min, now->tm_sec, buf);
}

int log_va_check_error(int rv, const char* format, va_list args) {
	if (rv >= 0) return 0;

	int savedErrno = errno;
	char* buf = 0;

	vasprintf(&buf, format, args);

	log_message(LLVL_ERROR, "%s (%s)", buf, strerror(savedErrno));

	return 1;
}
