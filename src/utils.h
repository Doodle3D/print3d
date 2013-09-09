#ifndef UTILS_H_SEEN
#define UTILS_H_SEEN

#ifdef __cplusplus
	extern "C" {
#endif

#include <inttypes.h>

char* number_to_string(int n);
uint16_t read_ns(const void* p);
uint32_t read_nl(const void* p);
void store_ns(void* p, uint16_t v);
void store_nl(void* p, uint32_t v);
uint32_t getMillis();
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y);
int readAndAppendAvailableData(int fd, char **buf, int *buflen, int timeout, int onlyOnce);
char *readFileContents(const char *file, int *size);
int equal(const char *s1, const char *s2);
int isAbsolutePath(const char *path);

#ifdef __cplusplus
	} //extern "C"
#endif

#endif /* ! UTILS_H_SEEN */
