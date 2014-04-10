/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef LOGGER_H_SEEN
#define LOGGER_H_SEEN

#include <stdio.h>
#include <stdarg.h>


#ifdef __cplusplus

class Logger {
public:
	typedef enum ELOG_LEVEL {
		INVALID = 0, //marker to detect uninitialized instances of this enum
		QUIET = 1, //not used for logging, only for setting levels
		ERROR = 2,
		WARNING = 3,
		INFO = 4,
		VERBOSE = 5,
		BULK = 6
	} ELOG_LEVEL;

	static Logger& getInstance();

	int open(FILE* stream, ELOG_LEVEL level);
	int open(const char* file, ELOG_LEVEL level);
	int close();

	bool isOpen() const;
	ELOG_LEVEL getLevel() const;
	void setLevel(ELOG_LEVEL level);
	void log(ELOG_LEVEL level, const char* format, ...) const;
	void vaLog(ELOG_LEVEL level, const char* format, va_list args) const;
	bool checkError(int rv, const char* format, ...) const;
	bool vaCheckError(int rv, const char* format, va_list args) const;
	void logIpcCmd(ELOG_LEVEL level, const char *buf, int buflen, bool isReply = false);

private:
	ELOG_LEVEL level_;
	FILE* stream_;

	Logger();

	Logger(const Logger& o);
	void operator=(const Logger& o);
};

#endif /* __cplusplus */

#endif /* ! LOGGER_H_SEEN */
