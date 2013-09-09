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

static int socketFd = -1;
static const char *error = NULL;
const char *comm_getError() { return error; }
static void clearError() { error = NULL; }
const void setError(const char *e) { error = e; }


int comm_openSocketForDeviceId(const char *deviceId) {
	struct sockaddr_un remote;
	int len;

	if (socketFd >= 0) return socketFd;

	socketFd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (log_check_error(socketFd, "could not create domain socket")) return -1;

	char *path = ipc_construct_socket_path(deviceId);
	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, path);
	len = sizeof(struct sockaddr_un);

	if (log_check_error(connect(socketFd, (struct sockaddr *)&remote, len), "could not connect domain socket with path '%s'", path)) {
		close(socketFd);
		free(path);
		socketFd = -1;
		return -1;
	}
	free(path);

	int flags = fcntl(socketFd, F_GETFL, 0);
	log_check_error(flags, "could not obtain flags for domain socket");
	log_check_error(fcntl(socketFd, F_SETFL, (flags | O_NONBLOCK)), "could not enable non-blocking mode on domain socket");

	signal(SIGPIPE, SIG_IGN);

	return socketFd;
}

int comm_closeSocket() {
	int rv = close(socketFd);
	socketFd = -1;
	return rv;
}

static char* sendAndReceiveData(const char *deviceId, const char *sbuf, int sbuflen, int *rbuflen) {
	if (socketFd < 0) return NULL;

	if (!sbuf) {
		log_message(LLVL_ERROR, "ipc command send buffer empty (construction failed?)");
		setError("ipc command construction failed");
		return NULL;
	}

	log_message(LLVL_VERBOSE, "sending command 0x%x (%i bytes)", ipc_cmd_get(sbuf, sbuflen), sbuflen);
	int rv = send(socketFd, sbuf, sbuflen, 0); //this should block until all data has been sent

	if (log_check_error(rv, "error sending ipc command 0x%x", ipc_cmd_get(sbuf, sbuflen))) {
		setError("could not send ipc command");
		return NULL;
	} else if (rv < sbuflen) {
		log_message(LLVL_WARNING, "could not send complete ipc command 0x%i (%i bytes written)", ipc_cmd_get(sbuf, sbuflen), rv);
		setError("could not send complete ipc command");
	} else {
		log_message(LLVL_VERBOSE, "ipc command 0x%x sent", ipc_cmd_get(sbuf, sbuflen));
	}

	char *rbuf = 0;
	*rbuflen = 0;
	//read only once to avoid unneccessary timeout but still allow for the timeout to happen
	log_check_error(readAndAppendAvailableData(socketFd, &rbuf, rbuflen, IPC_WAIT_TIMEOUT, 1), "error reading data from domain socket");

	return rbuf;
}

static int handleBasicResponse(char *scmd, int scmdlen, char *rcmd, int rcmdlen) {
	if (!rcmd) return -1; //NOTE: do not log anything, this is already in sendAndReceiveData()

	int rv = 0;
	log_message(LLVL_VERBOSE, "received %i bytes", rcmdlen);

	switch(ipc_cmd_get(rcmd, rcmdlen)) {
	case IPC_CMDR_OK:
		log_message(LLVL_VERBOSE, "received ipc reply 'OK' (%i bytes) in response to 0x%x", rcmdlen, ipc_cmd_get(rcmd, rcmdlen), ipc_cmd_get(scmd, scmdlen));
		break;
	case IPC_CMDR_ERROR:
		log_message(LLVL_VERBOSE, "received ipc reply 'ERROR' (%i bytes) in response to 0x%x", rcmdlen, ipc_cmd_get(rcmd, rcmdlen), ipc_cmd_get(scmd, scmdlen));
		setError("server returned error");
		rv = -1;
		break;
	default:
		log_message(LLVL_WARNING, "received unexpected IPC reply 0x%x for command 0x%x", ipc_cmd_get(rcmd, rcmdlen), ipc_cmd_get(scmd, scmdlen));
		setError("server returned unexpected response");
		rv = -1;
		break;
	}

	return rv;
}


int comm_testCommand(const char *deviceId, const char *question, char **answer) {
	clearError();

	int scmdlen, rcmdlen;

	char *scmd;
	if (question) scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_TEST, "s", question);
	else scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_TEST, 0);

	char *rcmd = sendAndReceiveData(deviceId, scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen) >= 0) {
		rv = ipc_cmd_get_string_arg(rcmd, rcmdlen, 0, answer);
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);

	return rv;
}

//returns 0 on success, -1 on error (retrieved using getError())
int comm_getTemperature(const char *deviceId, int16_t *temperature) {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GET_TEMPERATURE, 0);
	char *rcmd = sendAndReceiveData(deviceId, scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen) >= 0) {
		rv = ipc_cmd_get_short_arg(rcmd, rcmdlen, 0, temperature);
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);
	return rv;
}

int comm_clearGcode(const char *deviceId) {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_CLEAR, 0);
	char *rcmd = sendAndReceiveData(deviceId, scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen) >= 0) {
		log_message(LLVL_INFO, "gcode cleared");
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);
	return rv;
}


int comm_startPrintGcode(const char *deviceId) {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_STARTPRINT, 0);
	char *rcmd = sendAndReceiveData(deviceId, scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen) >= 0) {
		log_message(LLVL_VERBOSE, "gcode print started");
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);
	return rv;
}


int comm_stopPrintGcode(const char *deviceId) {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_STOPPRINT, 0);
	char *rcmd = sendAndReceiveData(deviceId, scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen) >= 0) {
		log_message(LLVL_VERBOSE, "gcode print stopped");
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);
	return rv;
}

int comm_sendGcodeFile(const char *deviceId, const char *file) {
	clearError();

	if (comm_clearGcode(deviceId) < 0) return -1;


	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND_FILE, "s", file);
	char *rcmd = sendAndReceiveData(deviceId, scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen) >= 0) {
		log_message(LLVL_VERBOSE, "gcode appended from file '%s'", file);
	} else {
		rv = -1;
	}


	if (rv >= 0) if (comm_clearGcode(deviceId) < 0) rv = -1;

	free(rcmd);
	free(scmd);
	return rv;
}

int comm_sendGcodeData(const char *deviceId, const char *gcode) {
	clearError();

//	fprintf(stderr, "datalen: %i; data: '%s'\n", strlen(gcode), gcode); //TEMP

	if (comm_clearGcode(deviceId) < 0) return -1;


	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND, "s", gcode);
	char *rcmd = sendAndReceiveData(deviceId, scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen) >= 0) {
		log_message(LLVL_VERBOSE, "gcode appended");
	} else {
		rv = -1;
	}


	if (rv >= 0) if (comm_clearGcode(deviceId) < 0) rv = -1;

	free(rcmd);
	free(scmd);
	return rv;
}
