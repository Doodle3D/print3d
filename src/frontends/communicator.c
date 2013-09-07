#include <fcntl.h>
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

static const int IPC_WAIT_TIMEOUT = 1500;

static const char* error = NULL;
const char* getError() { return error; }
static void clearError() { error = NULL; }
const void setError(const char* e) { error = e; }


static int openSocket(const char* path) {
	int fd = -1;
	struct sockaddr_un remote;
	int len;

	if (fd >= 0) return fd;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (log_check_error(fd, "could not open domain socket with path '%s'", path)) return -1;

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, path);
	len = sizeof(struct sockaddr_un);

	if (log_check_error(connect(fd, (struct sockaddr *)&remote, len), "could not connect domain socket with path '%s'", path)) {
		close(fd);
		fd = -1;
		return -1;
	}

	int flags = fcntl(fd, F_GETFL, 0);
	log_check_error(flags, "could not obtain flags for domain socket");
	log_check_error(fcntl(fd, F_SETFL, (flags | O_NONBLOCK)), "could not enable non-blocking mode on domain socket");

	signal(SIGPIPE, SIG_IGN);

	return fd;
}


static char* sendAndReceiveData(const char* deviceId, const char* buf, int buflen, int* rbuflen) {
	char* socketPath = ipc_construct_socket_path(deviceId);
	int fd = openSocket(socketPath);

	if (fd < 0) {
		free(socketPath);
		return NULL;
	}

	int rv = send(fd, buf, buflen, 0); //this should block until all data has been sent

	if (log_check_error(rv, "error sending ipc command 0x%x", ipc_cmd_get(buf, buflen))) {
		close(fd);
		free(socketPath);
		return NULL;
	} else if (rv < buflen) {
		log_message(LLVL_WARNING, "could not write complete ipc command 0x%i (%i bytes written)", ipc_cmd_get(buf, buflen), rv);
	} else {
		log_message(LLVL_VERBOSE, "ipc command 0x%x sent (%i bytes written)", ipc_cmd_get(buf, buflen), rv);
	}

	char* rbuf = 0;
	*rbuflen = 0;
	//read only once to avoid unneccessary timeout but still allow for the timeout to happen
	log_check_error(readAndAppendAvailableData(fd, &rbuf, rbuflen, IPC_WAIT_TIMEOUT, 1), "error reading data from domain socket");

	close(fd);
	free(socketPath);

	return rbuf;
}


int testCommand(const char* deviceId, const char* question, char** answer) {
	int rv, result = 0;

	log_open_stream(stderr, LLVL_VERBOSE);
	clearError();

	int cmdlen, rbuflen;

	char* cmdbuf;
	if (question) cmdbuf = ipc_construct_cmd(&cmdlen, IPC_CMDQ_TEST, "s", question);
	else cmdbuf = ipc_construct_cmd(&cmdlen, IPC_CMDQ_TEST, "");

	char* rbuf = sendAndReceiveData(deviceId, cmdbuf, cmdlen, &rbuflen);

	if (!rbuf) {
		setError("could not send/receive IPC command");
		free(cmdbuf);
		return -1;
	} else {
		log_message(LLVL_VERBOSE, "received %i bytes", rbuflen);
	}

	switch(ipc_cmd_get(rbuf, rbuflen)) {
	case IPC_CMDR_OK:
		rv = ipc_cmd_get_string_arg(rbuf, rbuflen, 0, answer);
		break;
	case IPC_CMDR_ERROR:
		setError("server returned error");
		result = -1;
		break;
	default:
		log_message(LLVL_WARNING, "received unexpected IPC reply 0x%x for command 0x%x", ipc_cmd_get(rbuf, rbuflen), ipc_cmd_get(cmdbuf, cmdlen));
		setError("server returned unexpected response");
		result = -1;
		break;
	}

	free(rbuf);
	free(cmdbuf);
	return result;
}

//returns 0 on success, -1 on error (retrieved using getError())
int getTemperature(const char* deviceId, int16_t* temperature) {
	int rv, result = 0;

	log_open_stream(stderr, LLVL_VERBOSE);
	clearError();

	int cmdlen, rbuflen;
	char* cmdbuf = ipc_construct_cmd(&cmdlen, IPC_CMDQ_GET_TEMPERATURE, 0);
	char* rbuf = sendAndReceiveData(deviceId, cmdbuf, cmdlen, &rbuflen);

	if (!rbuf) {
		setError("could not send/receive IPC command");
		free(cmdbuf);
		return -1;
	} else {
		log_message(LLVL_VERBOSE, "received %i bytes", rbuflen);
	}

	switch(ipc_cmd_get(rbuf, rbuflen)) {
	case IPC_CMDR_OK:
		rv = ipc_cmd_get_short_arg(rbuf, rbuflen, 0, temperature);
		break;
	case IPC_CMDR_ERROR:
		setError("server returned error");
		result = -1;
		break;
	default:
		log_message(LLVL_WARNING, "received unexpected IPC reply 0x%x for command 0x%x", ipc_cmd_get(rbuf, rbuflen), ipc_cmd_get(cmdbuf, cmdlen));
		setError("server returned unexpected response");
		result = -1;
		break;
	}

	free(rbuf);
	free(cmdbuf);
	return result;
}

//returns 'boolean' 1 on success or 0 on failure
//TODO: implement real call
int setTemperatureCheckInterval(const char* deviceId, int interval) {
	return 0;
}
