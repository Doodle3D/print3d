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

#define DEBUG_GCODE_FRAGMENTATION /** Uncomment to enable line counting while sending gcode data. */

static const int IPC_WAIT_TIMEOUT = 60 * 1000; ///How long to poll for input when we expect data (60*1000 = 1 minute).
static const int MAX_PACKET_SIZE = 1024 - 8; ///Largest packet which can make it through the ipc pipe (minus 8 bytes for cmdId+argNum+arg0Len).

static int socketFd = -1;
static const char *error = NULL;
const char *comm_getError() { return error; }
static void clearError() { error = NULL; }
static void setError(const char *e) { error = e; }


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

static char* sendAndReceiveData(const char *sbuf, int sbuflen, int *rbuflen) {
	if (socketFd < 0) return NULL;

	if (!sbuf) {
		log_message(LLVL_ERROR, "ipc command send buffer empty (construction failed?)");
		setError("ipc command construction failed");
		return NULL;
	}

	log_message(LLVL_VERBOSE, "sending command");
	log_ipc_cmd(LLVL_VERBOSE, sbuf, sbuflen, 0);
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

static int handleBasicResponse(char *scmd, int scmdlen, char *rcmd, int rcmdlen, int expectedArgCount) {
	if (!rcmd) return -1; //NOTE: do not log anything, this is already in sendAndReceiveData()

	int rv = 0;
	log_message(LLVL_VERBOSE, "received %i bytes", rcmdlen);

	switch(ipc_cmd_get(rcmd, rcmdlen)) {
	case IPC_CMDR_OK:
		log_message(LLVL_VERBOSE, "received ipc reply 'OK' (%i bytes) in response to 0x%x", rcmdlen, ipc_cmd_get(scmd, scmdlen));
		int numArgs = ipc_cmd_num_args(rcmd, rcmdlen);
		if (numArgs != expectedArgCount) {
			log_message(LLVL_ERROR, "received ipc response with %i arguments (expected %i)", numArgs, expectedArgCount);
			rv = -1;
		}
		break;
	case IPC_CMDR_ERROR: {
		char *errmsg = 0;
		ipc_cmd_get_string_arg(rcmd, rcmdlen, 0, &errmsg);
		log_message(LLVL_VERBOSE, "received ipc reply 'ERROR' (%i bytes) in response to 0x%x (%s)", rcmdlen, ipc_cmd_get(scmd, scmdlen), errmsg);
		setError("server returned error");
		free(errmsg);
		rv = -1;
		break;
	}
	default:
		log_message(LLVL_WARNING, "received unexpected IPC reply 0x%x for command 0x%x", ipc_cmd_get(rcmd, rcmdlen), ipc_cmd_get(scmd, scmdlen));
		setError("server returned unexpected response");
		rv = -1;
		break;
	}

	return rv;
}


int comm_testCommand(const char *question, char **answer) {
	clearError();

	int scmdlen, rcmdlen;

	char *scmd;
	if (question) scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_TEST, "x", question, strlen(question));
	else scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_TEST, 0);

	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 1) >= 0) {
		rv = ipc_cmd_get_string_arg(rcmd, rcmdlen, 0, answer);
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);

	return rv;
}

//returns 0 on success, -1 on error (retrieved using getError())
int comm_getTemperature(int16_t *temperature, IPC_TEMPERATURE_PATAMETER which) {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GET_TEMPERATURE, "w", which);
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 1) >= 0) {
		rv = ipc_cmd_get_short_arg(rcmd, rcmdlen, 0, temperature);
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);
	return rv;
}

int comm_clearGcode() {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_CLEAR, 0);
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0) >= 0) {
		log_message(LLVL_INFO, "gcode cleared");
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);
	return rv;
}


int comm_startPrintGcode() {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_STARTPRINT, 0);
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0) >= 0) {
		log_message(LLVL_VERBOSE, "gcode print started");
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);
	return rv;
}


int comm_stopPrintGcode(const char *endCode) {
	clearError();

	int scmdlen, rcmdlen;

	char *scmd = 0;

	if (!endCode) scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_STOPPRINT, "x", "", 0);
	else scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_STOPPRINT, "x", endCode, strlen(endCode));

	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0) >= 0) {
		log_message(LLVL_VERBOSE, "gcode print stopped");
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);
	return rv;
}

int comm_sendGcodeFile(const char *file) {
	clearError();

	if (comm_clearGcode() < 0) return -1;


	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND_FILE, "x", file, strlen(file));
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0) >= 0) {
		log_message(LLVL_VERBOSE, "gcode appended from file '%s'", file);
	} else {
		rv = -1;
	}


	if (rv >= 0) if (comm_clearGcode() < 0) rv = -1;

	free(rcmd);
	free(scmd);
	return rv;
}

int comm_sendGcodeData(const char *gcode) {
	clearError();

	if (comm_clearGcode() < 0) return -1;

	int rv = 0;
	const char *startP = gcode, *endP;
	const char *lastPos = gcode + strlen(gcode) - 1;
#ifdef DEBUG_GCODE_FRAGMENTATION
	int packetNum = 0, lineNum = 1;
#else
	int packetNum = 0;
#endif

	log_message(LLVL_INFO, "starting transmit of %i bytes of gcode data, maximum packet size is %i", strlen(gcode), MAX_PACKET_SIZE);
	for (;;) {
		if (startP > lastPos) break;

		endP = startP + MAX_PACKET_SIZE - 1;
		if (endP > lastPos) {
			endP = lastPos;
		} else {
			while (endP >= startP && *endP != '\n') endP--;
		}

		if (endP < startP) { //line appears to be longer than MAX_PACKET_SIZE...
#ifdef DEBUG_GCODE_FRAGMENTATION
			log_message(LLVL_ERROR, "comm_sendGcodeData: could not find line break within max packet boundary of %i bytes (offset: %i, approx. line num: %i, packet #%i)",
					MAX_PACKET_SIZE, startP - gcode, lineNum, packetNum);
#else
			log_message(LLVL_ERROR, "comm_sendGcodeData: could not find line break within max packet boundary of %i bytes (offset: %i, packet #%i)",
					MAX_PACKET_SIZE, startP - gcode, packetNum);
#endif
			rv = -1;
			break;
		}

#ifdef DEBUG_GCODE_FRAGMENTATION
		for (const char *sp = startP; sp <= endP; ++sp) if (*sp == '\n') lineNum++;
#endif

		int scmdlen, rcmdlen;
		char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND, "x", startP, endP - startP + 1);
		char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

		if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0) >= 0) {
			log_message(LLVL_BULK, "gcode packet #%i appended (%i bytes)", packetNum, endP - startP + 1);
		} else {
			rv = -1;
		}

		free(rcmd);
		free(scmd);
		if (rv == -1) {
			packetNum++; //to keep a correct count for debugging purposes
			break;
		}

		startP = endP + 1;
		packetNum++;
	}
	log_message(LLVL_INFO, "gcode data sent in %i packets", packetNum);


	if (rv >= 0) if (comm_startPrintGcode() < 0) rv = -1;

	return rv;
}

int comm_heatup(int temperature) {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_HEATUP, "w", temperature);
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);

	free(rcmd);
	free(scmd);
	return rv;
}

int comm_getProgress(int16_t *currentLine, int16_t *numLines) {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GET_PROGRESS, 0);
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 2) >= 0) {
		rv = ipc_cmd_get_short_arg(rcmd, rcmdlen, 0, currentLine);
		if (rv > -1) rv = ipc_cmd_get_short_arg(rcmd, rcmdlen, 1, numLines);
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);
	return rv;
}

int comm_getState(char **state) {
	clearError();

	int scmdlen, rcmdlen;

	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GET_STATE, 0);
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	if (handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 1) >= 0) {
		rv = ipc_cmd_get_string_arg(rcmd, rcmdlen, 0, state);
	} else {
		rv = -1;
	}

	free(rcmd);
	free(scmd);

	return rv;
}
