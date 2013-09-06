#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "utils.h"
#include <sys/time.h>
#include <time.h>

//returns a newly allocated ASCIIZ string, or NULL if an error occured
char* number_to_string(int n) {
	int asclen = (int)((ceil(log10(fabs(n))) + 2) * sizeof(char));
	char* argbuf = (char*)malloc(asclen);

	if (!argbuf) return NULL;

	if (snprintf(argbuf, asclen, "%i", n) < asclen) {
		return argbuf;
	} else {
		free(argbuf);
		return NULL;
	}
}

uint16_t read_ns(const char* p) {
	return ntohs(*p);
}

uint32_t read_nl(const char* p) {
	return ntohl(*p);
}

void store_ns(char* p, uint16_t v) {
	uint16_t nv = htons(v);
	memcpy(p, &nv, 2);
}

void store_nl(char* p, uint32_t v) {
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
