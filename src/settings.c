//add dependency on libuci
//- create settings.c and have it conditionally include uci.h
//- when uci.h is not included, return hardcoded fake values

#include <stddef.h>
#include "settings.h"

#ifdef USE_LIB_UCI
static struct uci_context *ctx = NULL;
#endif


int settings_init() {
#ifdef USE_LIB_UCI
//	if (ctx == NULL) ctx = uci_alloc_context();
	return (ctx != NULL) ? 1 : 0;
#else
	return 1;
#endif
}

int settings_deinit() {
#ifdef USE_LIB_UCI
//	if (ctx != NULL) uci_free_context(ctx);
	return 1;
#else
	return 1;
#endif
}

char *settings_get(const char *uci_spec) {
#ifdef USE_LIB_UCI
//	struct uci_ptr ptr;
//
//	if (uci_lookup_ptr(ctx, ptr, uci_spec, true) != UCI_OK) return NULL;
	return NULL;
#else
	return NULL;
#endif
}
