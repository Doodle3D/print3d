#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "communicator.h"
#include "../server/shared.h"
#include "logger.h"

static int openSocket(const char* path) {
	struct sockaddr_un remote;
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	int len;

	if (log_check_error(fd, "could not open domain socket with path '%s'", path)) return -1;

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, path);
	len = sizeof(struct sockaddr_un);

	if (log_check_error(connect(fd, (struct sockaddr *)&remote, len), "could not connect domain socket with path '%s'", path)) {
		close(fd);
		return -1;
	}

	return fd;
}

//returns newly allocated buffer (size is written to *len), arg is ignored atm
static char* constructCommand(const char* name, const char* arg, int* len) {
	if (len) *len = strlen(name) + 2;
	char* buf = (char*)malloc(*len);

	strcpy(buf, name);
	buf[*len - 2] = ';';
	buf[*len - 1] = '\0';

	return buf;
}

static char* constructSocketPath(const char* deviceId) {
	char* buf = (char*)malloc(strlen(SOCKET_PATH_PREFIX) + strlen(deviceId) + 1);
	strcpy(buf, SOCKET_PATH_PREFIX);
	strcat(buf, deviceId);
	return buf;
}

static char* sendAndReceiveData(const char* deviceId, COMMAND_INDICES idx, const char* arg) {
	char* socketPath = constructSocketPath(deviceId);
	int fd = openSocket(socketPath);

	if (fd < 0) {
		free(socketPath);
		return NULL;
	}

	int bufLen;
	char* sbuf = constructCommand(COMMAND_NAMES[idx], arg, &bufLen);
	bufLen--; //do not send the '\0'
	int rv = send(fd, sbuf, bufLen, 0); //this should block until all data has been sent

	if (log_check_error(rv, "error sending command '%s' to domain socket '%s'", sbuf, socketPath)) {
		close(fd);
		free(socketPath);
		free(sbuf);
		return NULL;
	} else if (rv < bufLen) {
		log_message(LLVL_WARNING, "could not write complete command '%s' (%i bytes written) to domain socket '%s'", sbuf, rv, socketPath);
	}

	char* rbuf = NULL;
	//TODO: now read input (use non-blocking socket + poll()? (see uhttpd))

	close(fd);
	free(socketPath);
	free(sbuf);

	return rbuf;
}


int getTemperature(const char* deviceId) {
	log_open_stream(stderr, LLVL_INFO);
	char* rbuf = sendAndReceiveData(deviceId, CMD_GET_TEMPERATURE_IDX, NULL);
	char* endptr;
	int temperature = INT_MIN;

	if (rbuf != NULL) temperature = strtol(rbuf, &endptr, 10);

	//TODO: check endptr

	free(rbuf);
	return temperature;
}
