#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "utils.h"

static const int READ_BUF_SIZE = 1024;
static const int READ_POLL_INTERVAL = 500;


//returns a newly allocated ASCIIZ string, or NULL if an error occured
char* number_to_string(int n) {
	int asclen = (int)((ceil(log10(fabs(n))) + 2) * sizeof(char));
	char *argbuf = (char*)malloc(asclen);

	if (!argbuf) return NULL;

	if (snprintf(argbuf, asclen, "%i", n) < asclen) {
		return argbuf;
	} else {
		free(argbuf);
		return NULL;
	}
}

uint16_t read_ns(const char *p) {
	return ntohs(*p);
}

uint32_t read_nl(const char *p) {
	return ntohl(*p);
}

void store_ns(char *p, uint16_t v) {
	uint16_t nv = htons(v);
	memcpy(p, &nv, 2);
}

void store_nl(char *p, uint32_t v) {
	uint32_t nv = htonl(v);
	memcpy(p, &nv, 4);
}

//from: http://www.gnu.org/software/libc/manual/html_node/Elapsed-Time.html
int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y) {
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait. tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

int readAndAppendAvailableData(int fd, char **buf, int *buflen) {
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	int newbuflen = 0, totalreadlen = 0;
	int rv;

	while(1) {
		newbuflen = *buflen + READ_BUF_SIZE;
		*buf = (char*)realloc(buf, newbuflen);
		rv = recv(fd, *buf + *buflen, newbuflen - *buflen, 0);

		if (rv < 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				//recv() would block...we wait using poll and then try again if data became available
				pfd.revents = 0;
				poll(&pfd, 1, READ_POLL_INTERVAL);

				if ((pfd.revents & POLLIN) == 0) break;
			} else if (errno != EINTR) {
				//ignore it if the call was interrupted (i.e. try again)
				break;
			}
		} else if (rv == 0) {
			//nothing to read anymore (remote end closed?)
			break;
		} else {
			*buflen += rv;
			totalreadlen += rv;
		}
	}

	if (*buflen > 0) {
		*buf = (char*)realloc(*buf, *buflen);
	} else {
		free(*buf);
		*buf = NULL;
	}

	return (rv > 0) ? totalreadlen : rv;
}
