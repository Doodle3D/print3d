#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "utils.h"
#include <sys/time.h>
#include <time.h>

static const int READ_BUF_SIZE = 1024;


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

uint16_t read_ns(const void *p) {
	return ntohs(*(uint16_t*)p);
}

uint32_t read_nl(const void *p) {
	return ntohl(*(uint32_t*)p);
}

void store_ns(void *p, uint16_t v) {
	uint16_t nv = htons(v);
	memcpy(p, &nv, 2);
}

void store_nl(void *p, uint32_t v) {
	uint32_t nv = htonl(v);
	memcpy(p, &nv, 4);
}

uint32_t getMillis() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
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

/** Read as much data and append to the given buffer.
 *
 * Note that the timeout will be reset each time any data could be read (i.e.,
 * only when no data was available, and nothing changed after the timeout, the
 * function will return).
 *
 * @param fd The file descriptor to read from
 * @param buf The buffer to read into, will be resized as necessary
 * @param buflen The buffer length, will be updated to reflect new buffer size
 * @param timeout How many milliseconds at most to wait for data (see note in function description)
 * @param onlyOnce If non-zero, do not loop anymore after any data has been read
 * @return >0 if data has been read and no errors occurred
 * @return 0 if no data could be read and no errors occurred
 * @return -1 if a read error occurred (errno contains details)
 * @return -2 if the file descriptor was closed
 */
int readAndAppendAvailableData(int fd, char **buf, int *buflen, int timeout, int onlyOnce) {
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	int newbuflen = 0, totalreadlen = 0;
	int rv;
  int exitValue;

	while(1) {
		newbuflen = *buflen + READ_BUF_SIZE;
		*buf = (char*)realloc(*buf, newbuflen);
		rv = read(fd, *buf + *buflen, newbuflen - *buflen);

		if (rv < 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				//recv() would block...if a timeout has been requested, we wait and then try again if data became available
				pfd.revents = 0;
				if (timeout > 0) poll(&pfd, 1, timeout);

				if ((pfd.revents & POLLIN) == 0) {
          exitValue = totalreadlen;
          break;
        }
			} else if (errno != EINTR) {
				//ignore it if the call was interrupted (i.e. try again)
        exitValue = -1;
				break;
			}
		} else if (rv == 0) {
			//nothing to read anymore (remote end closed?)
      exitValue = -2;
			break;
		} else {
			*buflen += rv;
			totalreadlen += rv;
      exitValue = totalreadlen;
      if (onlyOnce) break;
		}
	}

	if (*buflen > 0) {
		*buf = (char*)realloc(*buf, *buflen);
	} else {
		free(*buf);
		*buf = NULL;
	}

  return exitValue;
}

char *readFileContents(const char *file, int* size) {
	FILE* f = fopen(file, "rb");

	if (!f) return NULL;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	if (size) *size = fsize;
	fseek(f, 0, SEEK_SET);

	char *text = malloc(fsize + 1);
	fread(text, fsize, 1, f);

	text[fsize] = '\0';
	fclose(f);
	return text;
}
