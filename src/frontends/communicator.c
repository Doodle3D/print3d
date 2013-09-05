#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "communicator.h"
#include "../ipc_shared.h"
#include "../logger.h"
#include "../utils.h"

static int openSocket(const char* path) {
	static int fd_static = -1;
	struct sockaddr_un remote;
	int len;

	if (fd_static >= 0) return fd_static;

	fd_static = socket(AF_UNIX, SOCK_STREAM, 0);

	if (log_check_error(fd_static, "could not open domain socket with path '%s'", path)) return -1;

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, path);
	len = sizeof(struct sockaddr_un);

	if (log_check_error(connect(fd_static, (struct sockaddr *)&remote, len), "could not connect domain socket with path '%s'", path)) {
		close(fd_static);
		fd_static = -1;
		return -1;
	}

	int flags = fcntl(fd_static, F_GETFL, 0);
	log_check_error(flags, "could not obtain flags for domain socket");
	log_check_error(fcntl(fd_static, F_SETFL, (flags | O_NONBLOCK)), "could not enable non-blocking mode on domain socket");
	//int one = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	signal(SIGPIPE, SIG_IGN);

	return fd_static;
}

static const int READ_BUF_SIZE = 256;
static const int READ_POLL_INTERVAL = 500;

static char* readAvailableData(int fd, int* totallen) {
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	int actlen = 0, buflen = 0;
	char* buf = 0;

	while(1) {
		buflen = actlen + READ_BUF_SIZE;
		buf = (char*)realloc(buf, buflen);
		int rv = recv(fd, buf + actlen, buflen - actlen, 0);

		if (rv < 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				//recv() would block...we wait using poll and then try again if data became available
				pfd.revents = 0;
				poll(&pfd, 1, READ_POLL_INTERVAL);

				if ((pfd.revents & POLLIN) == 0) break;
			} else if (errno != EINTR) {
				//ignore it if the call was interrupted (i.e. try again)
				log_check_error(rv, "error reading data from domain socket");
				break;
			}
		} else if (rv == 0) {
			//nothing to read anymore (remote end closed?)
			break;
		} else {
			actlen += rv;
		}
	}

	//reallocate buffer to correct size and append nul-byte
	buf = (char*)realloc(buf, actlen + 1);
	if (totallen) *totallen = actlen + 1;
	buf[actlen] = '\0';

	return buf;
}

static char* sendAndReceiveData(const char* deviceId, IPC_COMMAND_CODE code, const char* arg) {
	char* socketPath = ipc_construct_socket_path(deviceId);
	int fd = openSocket(socketPath);

	if (fd < 0) {
		free(socketPath);
		return NULL;
	}

	int bufLen;
	//char* sbuf = ipc_construct_command(IPC_COMMAND_NAMES[idx], arg, &bufLen);
	char* sbuf = NULL; // reimplement using new IPC commands
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

	int rbuflen;
	char* rbuf = readAvailableData(fd, &rbuflen);

	close(fd);
	free(socketPath);
	free(sbuf);

	return rbuf;
}


//returns temperature, or INT_MIN on error
int getTemperature(const char* deviceId) {
	log_open_stream(stderr, LLVL_VERBOSE);
	char* rbuf = sendAndReceiveData(deviceId, IPC_CMD_GET_TEMPERATURE, NULL);
	char* endptr;
	int temperature = INT_MIN;

	if (rbuf != NULL) {
		temperature = strtol(rbuf, &endptr, 10);
		if (*endptr != '\0') log_message(LLVL_WARNING, "could not properly parse IPC response to getTemperature as number (parsed %i from '%s')", temperature, rbuf);
	}

	free(rbuf);
	return temperature;
}

//returns 'boolean' 1 on success or 0 on failure
//TODO: implement real call
int setTemperatureCheckInterval(const char* deviceId, int interval) {
//	log_open_stream(stderr, LLVL_VERBOSE);
//	char* ivalBuf = number_to_string(interval);
//	char* rbuf = sendAndReceiveData(deviceId, CMD_GET_TEMPERATURE_IDX, ivalBuf);
//	char* endptr;
//	int temperature = INT_MIN;
//
//	if (rbuf != NULL) {
//		temperature = strtol(rbuf, &endptr, 10);
//		if (*endptr != '\0') log_message(LLVL_WARNING, "could not properly parse IPC response to getTemperature as number (parsed %i from '%s')", temperature, rbuf);
//	}
//
//	free(ivalBuf);
//	free(rbuf);
//	return temperature;
}
