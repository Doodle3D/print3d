/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef SETTINGS_H_SEEN
#define SETTINGS_H_SEEN

#if UCI_ENABLED == 1
//#error "UCI_ENABLED" //--error disabled
//TODO: improve this to only detect openwrt
# ifdef __linux
#  include "uci.h"
#  define USE_LIB_UCI
# endif
#endif

#ifdef __cplusplus
	extern "C" {
#endif

int settings_init();
int settings_deinit();
const char *settings_get(const char *uci_spec);

#ifdef __cplusplus
	} //extern "C"
#endif

#endif /* ! SETTINGS_H_SEEN */
