#ifndef FE_CMDLINE_H_SEEN
#define FE_CMDLINE_H_SEEN

#include <stdio.h>

/* from fe_cmdline.c */
typedef enum ACTION_TYPE {
		AT_NONE, AT_ERROR, AT_SHOW_HELP,
		AT_GET_TEMPERATURE, AT_GET_TEST, AT_GET_PROGRESS, AT_GET_STATE, AT_GET_SUPPORTED,
		AT_HEATUP, AT_PRINT_FILE, AT_SEND_CODE, AT_STOP_PRINT
} ACTION_TYPE;

extern int verbosity;
extern char *deviceId;
extern char *print_file;
extern char *send_gcode;
extern char *end_gcode;
extern int heatup_temperature;

/* from actions.c */
int handle_action(int argc, char **argv, ACTION_TYPE action);

#endif /* ! FE_CMDLINE_H_SEEN */
