#ifndef UTILS_H_SEEN
#define UTILS_H_SEEN

#include <inttypes.h>

char* number_to_string(int n);
uint16_t read_ns(const char* p);
uint32_t read_nl(const char* p);
void store_ns(char* p, uint16_t v);
void store_nl(char* p, uint32_t v);

#endif /* ! UTILS_H_SEEN */
