#ifndef UTILS_H_SEEN
#define UTILS_H_SEEN

#ifdef __cplusplus
	extern "C" {
#endif

#include <inttypes.h>

char* number_to_string(int n);
uint16_t read_ns(const char* p);
uint32_t read_nl(const char* p);
void store_ns(char* p, uint16_t v);
void store_nl(char* p, uint32_t v);
uint32_t getMillis();
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y);
int readAndAppendAvailableData(int fd, char **buf, int *buflen);

#ifdef __cplusplus
	} //extern "C"
#endif

#endif /* ! UTILS_H_SEEN */
