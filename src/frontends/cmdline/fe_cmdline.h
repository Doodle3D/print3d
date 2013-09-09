#ifndef FE_CMDLINE_H_SEEN
#define FE_CMDLINE_H_SEEN

#include <stdio.h>

/* from fe_cmdline.c */
extern char *deviceId;
extern char *print_file;
extern char *send_gcode;

/* from actions.c */
void printTemperatureAction();
void printTestResponseAction();
void sendGcodeFileAction(const char *file);

#endif /* ! FE_CMDLINE_H_SEEN */
