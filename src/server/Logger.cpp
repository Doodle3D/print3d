/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "Logger.h"
#include "../ipc_shared.h"


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

void Logger::log(ELOG_LEVEL level, const char *module, const char *format, ...) const {
	va_list args;
	va_start(args, format);
	vaLog(level, module, format, args);
	va_end(args);
}

void Logger::vaLog(ELOG_LEVEL level, const char *module, const char *format, va_list args) const {
	time_t ltime;
	struct tm *now;

	if (!stream_ || level > level_) return;

	char* buf = 0;
	vasprintf(&buf, format, args);

	ltime = time(NULL);
	now = localtime(&ltime);
	fprintf(stream_, "%02i-%02i %02i:%02i:%02i [%s] (%s)%*s: %s\n",
			now->tm_mday, now->tm_mon + 1, now->tm_hour, now->tm_min, now->tm_sec,
			module, LOG_LEVEL_NAMES[level], getPaddingForLevel(level), "", buf);
	free(buf);
}

bool Logger::checkError(int rv, const char *module, const char *format, ...) const {
	va_list args;
	va_start(args, format);
	bool result = vaCheckError(rv, module, format, args);
	va_end(args);
	return result;
}

bool Logger::vaCheckError(int rv, const char *module, const char *format, va_list args) const {
	if (rv != -1) return false; //treat _only_ -1 as error (since we can only handle system errors)

	int savedErrno = errno;
	char *buf = 0;

	vasprintf(&buf, format, args);

	log(ERROR, module, "%s (%s)", buf, strerror(savedErrno));
	free(buf);

	return true;
}

void Logger::logIpcCmd(ELOG_LEVEL level, const char *buf, int buflen, bool isReply) {
	if (!stream_ || level > level_) return; //check this before calling command stringifier

	char *cmd_text = 0;
	ipc_stringify_cmd(buf, buflen, isReply ? 1 : 0, &cmd_text);

	log(level, "IPC ", "command: %s", cmd_text);

	free(cmd_text);
}

/*********************
 * PRIVATE FUNCTIONS *
 *********************/

Logger::Logger()
: level_(BULK), stream_(0)
{ /* empty */ }

int Logger::getPaddingForLevel(ELOG_LEVEL level) const {
	static int max_len = -1;

	if (max_len == -1) {
		for (int i = 1; i < NUM_LOG_LEVELS; ++i) {
			int l = strlen(LOG_LEVEL_NAMES[i]);
			if (l > max_len) max_len = l;
		}
	}

	return max_len - strlen(LOG_LEVEL_NAMES[level]);
}
