#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "Logger.h"

/* STATIC */
Logger& Logger::getInstance() {
	/* instance will be correctly destroyed but is NOT thread safe
	 * (see: http://stackoverflow.com/questions/1008019/c-singleton-design-pattern/1008289#1008289)
	 */
	static Logger instance;
	return instance;
}

int Logger::open(FILE* stream, ELOG_LEVEL level) {
	int rv = 0;
	//TODO: return error if not writable
	if ((rv = close()) >= 0) {
		stream_ = stream;
		setLevel(level);
	}

	return rv;
}

int Logger::open(const char* file, ELOG_LEVEL level) {
	int rv = 0;

	if ((rv = close()) >= 0) {
		//TODO: open filename (how to handle existing files?)
		setLevel(level);
	}

	return rv;
}

int Logger::close() {
	int rv = 0;

	if (stream_ != NULL) rv = fclose(stream_);

	return rv;
}

bool Logger::isOpen() const {
	return (stream_ != NULL);
}

Logger::ELOG_LEVEL Logger::getLevel() const {
	return level_;
}

void Logger::setLevel(ELOG_LEVEL level) {
	level_ = level;
}

void Logger::log(ELOG_LEVEL level, const char* format, ...) const {
	va_list args;
	time_t ltime;
	struct tm* now;

	if (!stream_ || level > level_) return;

	ltime = time(NULL);
	now = localtime(&ltime);
	fprintf(stream_, "%02i-%02i %02i:%02i:%02i  ",now->tm_mday, now->tm_mon + 1, now->tm_hour, now->tm_min, now->tm_sec);

	va_start(args, format);
	vfprintf(stream_, format, args);
	va_end(args);
}

bool Logger::checkError(int rv, const char* format, ...) const {
	if (rv >= 0) return false;

	int savedErrno = errno;
	va_list args;
	char *buf = 0;

	va_start(args, format);
	vasprintf(&buf, format, args);
	va_end(args);

	log(ERROR, "%s (%s)", buf, strerror(savedErrno));

	return true;
}


/*********************
 * PRIVATE FUNCTIONS *
 *********************/

Logger::Logger()
: level_(BULK), stream_(0)
{ /* empty */ }
