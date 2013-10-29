#ifndef SETTINGS_H_SEEN
#define SETTINGS_H_SEEN

//TODO: improve this to only detect openwrt
#ifdef __linux
# include "uci.h"
# define USE_LIB_UCI
#endif

#ifdef __cplusplus
	extern "C" {
#endif

int settings_init();
int settings_deinit();
char *settings_get(const char *uci_spec);

#ifdef __cplusplus
	} //extern "C"
#endif

#endif /* ! SETTINGS_H_SEEN */
