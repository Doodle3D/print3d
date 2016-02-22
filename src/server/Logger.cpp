/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <algorithm>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include "Logger.h"
#include "../ipc_shared.h"

using std::string;


const string Logger::LOG_FILE_EXTENSION = ".log";

/* STATIC */
Logger& Logger::getInstance() {
	/* instance will be correctly destroyed but is NOT thread safe
	 * (see: http://stackoverflow.com/questions/1008019/c-singleton-design-pattern/1008289#1008289)
	 */
	static Logger instance;
	return instance;
}

int Logger::open(FILE *stream, ELOG_LEVEL level) {
	int rv = 0;

	if (!stream) return -2;

	if ((rv = close()) >= 0) {
		//TODO: use freopen to ensure stream is writable?
		stream_ = stream;
		setlinebuf(stream_);
		setLevel(level);
	}

	return rv;
}

int Logger::open(const string *file, ELOG_LEVEL level) {
	int rv = 0;

	if (!file) return -2;

	if ((rv = close()) >= 0) {
		stream_ = fopen(file->c_str(), "a");
		if (stream_ == NULL) return -1;

		setlinebuf(stream_);
		setLevel(level);
	}

	return rv;
}

/*
 * Opens a log stream based on the given parameters.
 * Basename can have two special meanings:
 * If it is '<...>', the text in between can be either stdout or stderr to log to that standard stream.
 * If it is '`...`', the text in between will be executed as a command using popen; for instance to log over ssh, use: `ssh localhost "cat > /tmp/print3d.log"`.
 * Otherwise the log file that will be opened is: '<path>/<basename>[-<log_id>].log'.
 *
 * @param path The absolute path the log file should reside in
 * @param basename Either the basename of the logfile, which will be suffixed with '.log', or <...> to use stdout or stderr
 * @param logId Null, or a string to be infixed to the log filename
 * @return 0 on success, -1 on system error (errno will be set) or -2 if no name/stdstream could be determined from the parameters
 */
int Logger::openParameterized(const string *path, const string *basename, const string *logId, ELOG_LEVEL level) {
	if (!basename || basename->empty()) return -2;

	int rv = 0;
	FILE *logstream = 0;

	char first = *basename->begin(), last = *basename->rbegin();
	string spec = basename->substr(1, basename->length() - 2);

	if (first == '<' && last == '>') { //interpret <...> as a standard stream
		std::transform(spec.begin(), spec.end(),spec.begin(), ::tolower);

		if (spec == "stdout") logstream = stdout;
		else if (spec == "stderr") logstream = stderr;

		rv = open(logstream, level);
	} else if (first == '`' && last == '`') { //interpret `...` as a command to popen()
		close();
		FILE *st = popen(spec.c_str(), "w");

		if (st) {
			stream_ = st;
			setLevel(level);
			setlinebuf(stream_);
			streamIsPipe_ = true;
		} else {
			rv = -1;
		}
	} else if (path && path->at(0) == '/') { //else, if path is absolute, open a file
		string fullFilePath = *path + "/" + *basename;
		if (logId) fullFilePath += "-" + *logId;
		fullFilePath += LOG_FILE_EXTENSION;

		 rv = open(&fullFilePath, level);
	} else { //otherwise fail
		rv = -2;
	}

	return rv;
}

int Logger::openParameterized(const char *path, const char *basename, const char *logId, ELOG_LEVEL level) {
	string p = path ? string(path) : "";
	string b = basename ? string(basename) : "";
	string l = logId ? string(logId) : "";
	return openParameterized(path ? &p : 0, basename ? &b : 0, logId ? &l : 0, level);
}

int Logger::close() {
	int rv = 0;

	if (stream_ != 0) {
		rv = streamIsPipe_ ? pclose(stream_) : fclose(stream_);
		streamIsPipe_ = false;
		stream_ = 0;
	}

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

//static
Logger::ELOG_LEVEL Logger::getLevelForName(const std::string& name, ELOG_LEVEL defaultLevel) {
	if (name == "") return defaultLevel;

	int i = 1; //start at 1, skip invalid level
	while (LOG_LEVEL_NAMES[i] != NULL) {
		if (strcmp(name.c_str(), LOG_LEVEL_NAMES[i]) == 0) return (ELOG_LEVEL)i;
		i++;
	}
	return INVALID;
}


/*********************
 * PRIVATE FUNCTIONS *
 *********************/

Logger::Logger()
: level_(BULK), stream_(0), streamIsPipe_(false)
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
