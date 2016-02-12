/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "communicator.h"
#include "../ipc_shared.h"
#include "../logger.h"
#include "../utils.h"

static const char *MOD_ABBR = "COMM";

//NOTE: see Server.cpp for comments on these macros
#define LOG(lvl, fmt, ...) log_message(lvl, MOD_ABBR, fmt, ##__VA_ARGS__)


#define DEBUG_GCODE_FRAGMENTATION /** Uncomment to enable line counting while sending gcode data. */

/***********
 * STATICS *
 ***********/

static const int IPC_WAIT_TIMEOUT = 60 * 1000; ///How long to poll for input when we expect data (60*1000 = 1 minute).
static const int MAX_PACKET_SIZE = 1024 - 8; ///Largest packet which can make it through the ipc pipe (minus 8 bytes for cmdId+argNum+arg0Len).

//Note: these names are used all the way on the other end in javascript, consider this when changing them.
static const char *TRANSACTION_CANCELLED_STRING = "transaction_cancelled";
static const char *RETRY_LATER_STRING = "retry_later";

static int socketFd = -1;
static char *error = NULL;


static void clearError() {
	free(error);
	error = NULL;
}

static void setError(const char *format, ...) {
	if (error) clearError();
	va_list args;
	va_start(args, format);
	vasprintf(&error, format, args);
	va_end(args);
}

static int openSocketInternal(const char *deviceId) {
	struct sockaddr_un remote;
	int len;
	int fd;

	if (!deviceId) {
		LOG(LLVL_ERROR, "openSocketInternal(): missing device ID");
		return -1;
	}

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (log_check_error(fd, MOD_ABBR, "could not create domain socket")) return -1;

	char *path = ipc_construct_socket_path(deviceId);
	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, path);
	len = sizeof(struct sockaddr_un);

	if (log_check_error(connect(fd, (struct sockaddr *)&remote, len), MOD_ABBR, "could not connect domain socket with path '%s'", path)) {
		close(fd);
		free(path);
		fd = -1;
		return -1;
	}
	free(path);

	int flags = fcntl(fd, F_GETFL, 0);
	log_check_error(flags, MOD_ABBR, "could not obtain flags for domain socket");
	log_check_error(fcntl(fd, F_SETFL, (flags | O_NONBLOCK)), MOD_ABBR, "could not enable non-blocking mode on domain socket");

	signal(SIGPIPE, SIG_IGN);

	return fd;
}

static int closeSocketInternal(int fd) {
	return close(fd);
}

static char* sendAndReceiveDataWithFd(int fd, const char *sbuf, int sbuflen, int *rbuflen) {
	if (fd < 0) return NULL;

	if (!sbuf) {
		LOG(LLVL_ERROR, "ipc command send buffer empty (construction failed?)");
		setError("ipc command construction failed");
		return NULL;
	}

	log_ipc_cmd(LLVL_VERBOSE, sbuf, sbuflen, 0);
	int rv = send(fd, sbuf, sbuflen, 0); //this should block until all data has been sent

	if (log_check_error(rv, MOD_ABBR, "error sending ipc command 0x%x", ipc_cmd_get(sbuf, sbuflen))) {
		setError("could not send ipc command");
		return NULL;
	} else if (rv < sbuflen) {
		LOG(LLVL_ERROR, "could not send complete ipc command 0x%i (%i bytes written)", ipc_cmd_get(sbuf, sbuflen), rv);
		setError("could not send complete ipc command");
	}

	char *rbuf = 0;
	*rbuflen = 0;
	//read only once to avoid unneccessary timeout but still allow for the timeout to happen
	log_check_error(readAndAppendAvailableData(fd, &rbuf, rbuflen, IPC_WAIT_TIMEOUT, 1), MOD_ABBR, "error reading data from domain socket");

	return rbuf;
}

static char* sendAndReceiveData(const char *sbuf, int sbuflen, int *rbuflen) {
	return sendAndReceiveDataWithFd(socketFd, sbuf, sbuflen, rbuflen);
}

//returns 0 on success, -1 on error, -2 if appending gcode failed, -3 if transaction was cancelled, -4 on retryable error
//NOTE: the -2 case contains a formal error message which is set; -3 and -4 cases also have a formal error set (which is defined locally)
static int handleBasicResponse(char *scmd, int scmdlen, char *rcmd, int rcmdlen, int expectedArgCount) {
	if (!rcmd) return -1; //NOTE: do not log anything, this is already in sendAndReceiveData()

	int rv = 0;
	log_ipc_cmd(LLVL_VERBOSE, rcmd, rcmdlen, 1);

	switch(ipc_cmd_get(rcmd, rcmdlen)) {
		case IPC_CMDR_OK: {
			//LOG(LLVL_VERBOSE, "received ipc reply 'OK' (%i bytes) in response to 0x%x", rcmdlen, ipc_cmd_get(scmd, scmdlen));
			int numArgs = ipc_cmd_num_args(rcmd, rcmdlen);
			if (numArgs != expectedArgCount) {
				LOG(LLVL_ERROR, "received ipc response with %i arguments (expected %i)", numArgs, expectedArgCount);
				rv = -1;
			}
			break;
		}
		case IPC_CMDR_ERROR: {
			char *errmsg = 0;
			ipc_cmd_get_string_arg(rcmd, rcmdlen, 0, &errmsg);
			//LOG(LLVL_VERBOSE, "received ipc reply 'ERROR' (%i bytes) in response to 0x%x (%s)", rcmdlen, ipc_cmd_get(scmd, scmdlen), errmsg);
			setError("server returned error (%s)", errmsg);
			free(errmsg);
			rv = -1;
			break;
		}
		case IPC_CMDR_GCODE_ADD_FAILED: {
			char *errmsg = 0;
			ipc_cmd_get_string_arg(rcmd, rcmdlen, 0, &errmsg);
			setError(errmsg);
			free(errmsg);
			rv = -2;
			break;
		}
		case IPC_CMDR_TRX_CANCELLED: {
			setError(TRANSACTION_CANCELLED_STRING);
			rv = -3;
			break;
		}
		case IPC_CMDR_RETRY_LATER: {
			setError(RETRY_LATER_STRING);
			rv = -4;
			break;
		}
		default:
			LOG(LLVL_WARNING, "received unexpected IPC reply 0x%x for command 0x%x", ipc_cmd_get(rcmd, rcmdlen), ipc_cmd_get(scmd, scmdlen));
			setError("server returned unexpected response");
			rv = -1;
			break;
	}

	return rv;
}


/**********************
 * EXPORTED FUNCTIONS *
 **********************/

int comm_openSocket(const char *deviceId) {
	if (socketFd >= 0) return socketFd;

	socketFd = openSocketInternal(deviceId);
	return socketFd;
}

int comm_closeSocket() {
	int rv = closeSocketInternal(socketFd);
	socketFd = -1;
	return rv;
}

int comm_is_socket_open() {
	return socketFd >= 0 ? 1 : 0;
}

const char *comm_getError() {
	return error;
}


int comm_testCommand(const char *question, char **answer) {
	clearError();

	int scmdlen, rcmdlen;

	char *scmd;
	if (question) scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_TEST, "x", question, strlen(question));
	else scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_TEST, 0);

	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	int result = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 1);
	if (result >= 0) {
		rv = ipc_cmd_get_string_arg(rcmd, rcmdlen, 0, answer);
	} else {
		rv = result;
	}

	free(rcmd);
	free(scmd);

	return rv;
}

//TODO: with a mock gcodebuffer (or have it log somehow) we could also test if code is appended in expected order at expected moments
int comm_testTransactions(const char *deviceId, char **outputText) {
	int16_t trx_bits = 0;
	char *rcmd = 0, *scmd = 0;
	int rcmdlen = 0, scmdlen = 0;
	int rv;
	const char *data = "123456789", *stopData = "stop_gcode", *dummyPath = "/tmp/i.dont.exist";
	static const int NUM_TESTS = 6;

	*outputText = (char*)malloc(NUM_TESTS * 2 + 1);
	strcpy(*outputText, "");

	int fd1 = openSocketInternal(deviceId);
	int fd2 = openSocketInternal(deviceId);

	//send first three characters as first chunk using fd1 (expect ok)
	trx_bits = TRX_FIRST_CHUNK_BIT;
	scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND, "xw", data, 3, trx_bits);
	rcmd = sendAndReceiveDataWithFd(fd1, scmd, scmdlen, &rcmdlen);
	rv = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (rv >= 0) strcat(*outputText, "0+");
	else strcat(*outputText, "0-");
	free(rcmd); free(scmd);

	//send file to append using fd1 (expect retry_later, because transaction is already in progress)
	scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND_FILE, "x", dummyPath, strlen(dummyPath));
	rcmd = sendAndReceiveDataWithFd(fd1, scmd, scmdlen, &rcmdlen);
	rv = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (rv == -3) strcat(*outputText, "1+");
	else strcat(*outputText, "1-");
	free(rcmd); free(scmd);

	//send next three characters as first chunk using fd1 (expect ok)
	trx_bits = 0;
	scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND, "xw", data + 3, 3, trx_bits);
	rcmd = sendAndReceiveDataWithFd(fd1, scmd, scmdlen, &rcmdlen);
	rv = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (rv >= 0) strcat(*outputText, "2+");
	else strcat(*outputText, "2-");
	free(rcmd); free(scmd);

	//send last three characters as first chunk using fd2 (expect ok)
	trx_bits = TRX_FIRST_CHUNK_BIT;
	scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND, "xw", data + 6, 3, trx_bits);
	rcmd = sendAndReceiveDataWithFd(fd2, scmd, scmdlen, &rcmdlen);
	rv = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (rv >= 0) strcat(*outputText, "3+");
	else strcat(*outputText, "3-");
	free(rcmd); free(scmd);

	//send last three characters as first chunk using fd1 (expect ok)
	trx_bits = TRX_LAST_CHUNK_BIT;
	scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND, "xw", data + 6, 3, trx_bits);
	rcmd = sendAndReceiveDataWithFd(fd1, scmd, scmdlen, &rcmdlen);
	rv = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (rv >= 0) strcat(*outputText, "4+");
	else strcat(*outputText, "4-");
	free(rcmd); free(scmd);

	//send stop gcode using fd1 (expect ok)
	scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_STOPPRINT, "x", stopData, strlen(stopData));
	rcmd = sendAndReceiveDataWithFd(fd1, scmd, scmdlen, &rcmdlen);
	rv = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (rv >= 0) strcat(*outputText, "5+");
	else strcat(*outputText, "5-");
	free(rcmd); free(scmd);

	//send middle three characters using fd2 (expect cancelled, because fd1 stop cancelled all connected clients)
	trx_bits = 0;
	scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND, "xw", data + 3, 3, trx_bits);
	rcmd = sendAndReceiveDataWithFd(fd2, scmd, scmdlen, &rcmdlen);
	rv = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (rv == -2) strcat(*outputText, "6+");
	else strcat(*outputText, "6-");
	free(rcmd); free(scmd);

	closeSocketInternal(fd1);
	closeSocketInternal(fd2);

	return 0;
}

int comm_clearGCode() {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_CLEAR, 0);
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	int result = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (result >= 0) {
		LOG(LLVL_INFO, "gcode cleared");
	} else {
		rv = result;
	}

	free(rcmd);
	free(scmd);
	return rv;
}


int comm_startPrintGCode() {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_STARTPRINT, 0);
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	int result = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (result >= 0) {
		LOG(LLVL_INFO, "gcode print started");
	} else {
		rv = result;
	}

	free(rcmd);
	free(scmd);
	return rv;
}


int comm_stopPrintGCode(const char *endCode) {
	clearError();

	int scmdlen, rcmdlen;

	char *scmd = 0;

	if (!endCode) scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_STOPPRINT, "x", "", 0);
	else scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_STOPPRINT, "x", endCode, strlen(endCode));

	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	int result = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (result >= 0) {
		LOG(LLVL_INFO, "gcode print stopped");
	} else {
		rv = result;
	}

	free(rcmd);
	free(scmd);
	return rv;
}

int comm_sendGCodeFile(const char *file) {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND_FILE, "x", file, strlen(file));
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	int result = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
	if (result >= 0) {
		LOG(LLVL_INFO, "gcode appended from file '%s'", file);
	} else {
		rv = result;
	}

	free(rcmd);
	free(scmd);
	return rv;
}

//metadata may be NULL
int comm_sendGCodeData(const char *gcode, ipc_gcode_metadata_s *metadata) {
	uint32_t startTime = getMillis();
	clearError();

	int16_t seq_num = metadata ? metadata->seq_number : -1;
	int16_t seq_ttl = metadata ? metadata->seq_total : -1;

	int rv = 0;
	const char *startP = gcode, *endP;
	const char *lastPos = gcode + strlen(gcode) - 1;
#ifdef DEBUG_GCODE_FRAGMENTATION
	int packetNum = 0, lineNum = 1;
#else
	int packetNum = 0;
#endif

	LOG(LLVL_INFO, "starting transmit of %i bytes of gcode data, maximum packet size is %i", strlen(gcode), MAX_PACKET_SIZE);
	for (;;) {
		if (startP > lastPos) break;

		endP = startP + MAX_PACKET_SIZE - 1;
		if (endP >= lastPos) {
			endP = lastPos;
		} else {
			while (endP >= startP && *endP != '\n') endP--;
		}

		if (endP < startP) { //line appears to be longer than MAX_PACKET_SIZE...
#ifdef DEBUG_GCODE_FRAGMENTATION
			LOG(LLVL_ERROR, "comm_sendGcodeData: could not find line break within max packet boundary of %i bytes (offset: %i, approx. line num: %i, packet #%i)",
					MAX_PACKET_SIZE, startP - gcode, lineNum, packetNum);
#else
			LOG(LLVL_ERROR, "comm_sendGcodeData: could not find line break within max packet boundary of %i bytes (offset: %i, packet #%i)",
					MAX_PACKET_SIZE, startP - gcode, packetNum);
#endif
			rv = -1;
			break;
		}

#ifdef DEBUG_GCODE_FRAGMENTATION
		for (const char *sp = startP; sp <= endP; ++sp) if (*sp == '\n') lineNum++;
#endif

		int scmdlen, rcmdlen;
		int16_t trx_bits = 0;
		if (packetNum == 0) trx_bits |= TRX_FIRST_CHUNK_BIT;
		if (endP == lastPos) trx_bits |= TRX_LAST_CHUNK_BIT;

		char *scmd = 0;
		if (!metadata || !metadata->source) scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND, "xwWW", startP, endP - startP + 1, trx_bits, seq_num, seq_ttl);
		else scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GCODE_APPEND, "xwWWs", startP, endP - startP + 1, trx_bits, seq_num, seq_ttl, metadata->source);

		char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);
		int result = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 0);
		if (result >= 0) {
			LOG(LLVL_BULK, "gcode packet #%i transmitted in transaction (%i bytes)", packetNum, endP - startP + 1);
		} else {
			rv = result;
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
	uint32_t endTime = getMillis();

	if (rv == 0) LOG(LLVL_INFO, "gcode data sent in %i packets (%lu msecs)", packetNum, endTime - startTime);
	else LOG(LLVL_ERROR, "gcode data sent in %i packets (%lu msecs) until error %i", packetNum, endTime - startTime, rv);

	return rv;
}

//returns 0 on success, -1 on error (retrieved using getError())
int comm_getTemperature(int16_t *temperature, IPC_TEMPERATURE_PARAMETER which) {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GET_TEMPERATURE, "w", which);
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	int result = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 1);
	if (result >= 0) {
		rv = ipc_cmd_get_short_arg(rcmd, rcmdlen, 0, temperature);
	} else {
		rv = result;
	}

	free(rcmd);
	free(scmd);
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

int comm_getProgress(int32_t *currentLine, int32_t *bufferedLines, int32_t *totalLines, int32_t *bufferSize, int32_t *maxBufferSize) {
	clearError();

	int scmdlen, rcmdlen;
	char *scmd = ipc_construct_cmd(&scmdlen, IPC_CMDQ_GET_PROGRESS, 0);
	char *rcmd = sendAndReceiveData(scmd, scmdlen, &rcmdlen);

	int rv = 0;
	int result = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 5);
	if (result >= 0) {
		rv = ipc_cmd_get_long_arg(rcmd, rcmdlen, 0, currentLine);
		if (rv > -1) rv = ipc_cmd_get_long_arg(rcmd, rcmdlen, 1, bufferedLines);
		if (rv > -1) rv = ipc_cmd_get_long_arg(rcmd, rcmdlen, 2, totalLines);
		if (rv > -1) rv = ipc_cmd_get_long_arg(rcmd, rcmdlen, 3, bufferSize);
		if (rv > -1) rv = ipc_cmd_get_long_arg(rcmd, rcmdlen, 4, maxBufferSize);
	} else {
		rv = result;
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
	int result = handleBasicResponse(scmd, scmdlen, rcmd, rcmdlen, 1);
	if (result >= 0) {
		rv = ipc_cmd_get_string_arg(rcmd, rcmdlen, 0, state);
	} else {
		rv = result;
	}

	free(rcmd);
	free(scmd);

	return rv;
}
