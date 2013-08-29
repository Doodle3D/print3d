#ifndef LOGGER_H_SEEN
#define LOGGER_H_SEEN

#include <stdio.h>

class Logger {
public:
	typedef enum ELOG_LEVEL {
		QUIET = -1,
		ERROR = 0,
		WARNING = 1,
		INFO = 2,
		VERBOSE = 3,
		BULK = 4
	} ELOG_LEVEL;

	Logger();

	int open(FILE* stream, ELOG_LEVEL level);
	int open(const char* file, ELOG_LEVEL level);
	int close();

	bool isOpen() const;
	ELOG_LEVEL getLevel() const;
	void setLevel(ELOG_LEVEL level);
	void log(ELOG_LEVEL level, const char* format, ...) const;

private:
	ELOG_LEVEL level_;
	FILE* stream_;
};

#endif /* ! LOGGER_H_SEEN */
