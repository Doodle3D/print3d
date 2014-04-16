/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

/** @file ipc_shared.h
 * Functions related to IPC between server and frontends.
 *
 * This is all plain C code.
 */
#ifndef IPC_SHARED_H
#define IPC_SHARED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
/* #include <varargs.h> //varargs.h has been superseded by stdarg.h */
#include <inttypes.h>

typedef enum IPC_COMMAND_CODE {
	/* special codes (not sent or received) */
	IPC_CMDS_INVALID = 0,
	IPC_CMDS_NONE = 1,

	/* request commands sent by clients */
	IPC_CMDQ_TEST = 0x10,
	IPC_CMDQ_GET_TEMPERATURE,
	IPC_CMDQ_GCODE_CLEAR,
	IPC_CMDQ_GCODE_APPEND,
	IPC_CMDQ_GCODE_APPEND_FILE,
	IPC_CMDQ_GCODE_STARTPRINT,
	IPC_CMDQ_GCODE_STOPPRINT,
	IPC_CMDQ_HEATUP,
	IPC_CMDQ_GET_PROGRESS,
	IPC_CMDQ_GET_STATE,

	/* response commands send by server */
	IPC_CMDR_OK = 0xE0,
	IPC_CMDR_ERROR = 0xE1,
	IPC_CMDR_NOT_IMPLEMENTED = 0xE2,
	IPC_CMDR_GCODE_ADD_FAILED = 0xE3,
	IPC_CMDR_RETRY_LATER = 0xE4,
	IPC_CMDR_TRX_CANCELLED = 0xE5
} IPC_COMMAND_CODE;

typedef enum IPC_TEMPERATURE_PARAMETER {
	IPC_TEMP_NONE = 0,
	IPC_TEMP_HOTEND = 1,
	IPC_TEMP_HOTEND_TGT,
	IPC_TEMP_BED,
	IPC_TEMP_BED_TGT,
} IPC_TEMPERATURE_PATAMETER;

/** Structure to associate an IPC command code with metadata.
 *
 * The specified meta-data is a human-readable name, a description of the
 * expected arguments and a description of the expected response arguments (in
 * case #IPC_CMDR_OK is returned).
 *
 * Argument descriptions consist of a single character for each argument (either
 * on of {b,w,W,s,x}), '*' or '-'. '*' means argument count and types are
 * unspecified, '-' means the command is not valid to send/receive at all.
 * NOTE: 'b' has been removed.
 *
 * If #IPC_COMMAND_CODE.reply_fmt is NULL, no response is expected (typically
 * for commands which are responses themselves).
 */
typedef struct ipc_cmd_name_s {
	IPC_COMMAND_CODE code;	/// The code as used in IPC messages
	const char* name;				/// The corresponding human-readable name
	const char* arg_fmt;		/// printf-like argument requirements for command
	const char* reply_fmt;	/// printf-like argument requirements for response
} ipc_cmd_name_s;

/** Structure to pass around consistency data for GCode chunks.
 *
 * Each chunk has a sequence number which must increment by one each time until a buffer clear request has been made.
 * The total must stay the same until a buffer clear request has been made.
 * The source must must also be the same text each time until a clear request.
 * Note that all fields may be set to -1/NULL to have them ignored, but if one is
 * used once, it must be used consistently until a clear request.
 */
typedef struct ipc_gcode_metadata_s {
	int16_t seq_number;
	int16_t seq_total;
	const char *source;
} ipc_gcode_metadata_s;

/** Definitions of names for available IPC commands.
 */
extern const ipc_cmd_name_s IPC_COMMANDS[];

/** Transaction bits for use with the IPC_CMDQ_GCODE_APPEND command.
 * Only use bits 0-14 (must fit in int16_t).
 */
typedef enum IPC_GCODE_TRANSACTION_BITS {
	TRX_FIRST_CHUNK_BIT = 0x1,
	TRX_LAST_CHUNK_BIT = 0x2
} IPC_GCODE_TRANSACTION_BITS;

extern const char *IPC_SOCKET_PATH_PREFIX;
extern const char *IPC_DEFAULT_DEVICE_ID;

/** Constructs an IPC socket path for the given deviceId on which a
 * corresponding server should listen and clients should connect to.
 *
 * @param deviceId the identifier of the printer to create a socket path for.
 * @return A newly allocated string
 */
char* ipc_construct_socket_path(const char* deviceId);

/** Returns a list with all device paths likely to point to a printer.
 * (Actually, common usb-serial device names are collected.)
 * The list can be freed using @{free_device_list}.
 *
 * @return A NULL-terminated list containing the full path to each device found.
 * @retval NULL on system error
 */
char **ipc_find_devices();

/** Frees a device list created by @{find_devices}.
 * Note that the given list pointer will be invalid after calling this.
 * @param list The list to be freed.
 */
void ipc_free_device_list(char **list);

//all-in-one function to construct IPC commands in printf style
//please note the varargs are implemented by evil magic...
char* ipc_construct_cmd(int* cmdlen, IPC_COMMAND_CODE code, const char* fmtp, ...);

//all-in-one function to construct IPC commands in printf style
//please note the varargs are implemented by evil magic...
char* ipc_va_construct_cmd(int* cmdlen, IPC_COMMAND_CODE code, const char* format, va_list args);

//reallocates buf to 4 bytes (2 for command code and 2 argument count)
int ipc_cmd_set(char** buf, int* buflen, IPC_COMMAND_CODE code);

//reallocates buf to make room for arg (4 bytes for length + the argument itself)
//if arg is NULL, an empty argument is added
int ipc_cmd_add_arg(char** buf, int* buflen, const void* arg, uint32_t arglen);

/** Returns the total command size if buffer contains a complete command, or 0.
 *
 * @param buf Command buffer
 * @param buflen Command buffer length
 * @retval >0 the command size, if a complete command is present
 * @retval 0 if no complete command is present
 */
int ipc_cmd_is_complete(const char* buf, int buflen);

/** Returns the number of arguments in the command.
 *
 * @param buf Command buffer
 * @param buflen Command buffer length
 * @retval >=0 the number of command arguments
 * @retval -2 if the command is invalid
 */
int ipc_cmd_num_args(const char* buf, int buflen);

/** Extracts the command code.
 *
 * @param buf Command buffer
 * @param buflen Command buffer length
 * @return The command code
 * @retval #IPC_CMD_INVALID if the command is invalid
 */
IPC_COMMAND_CODE ipc_cmd_get(const char* buf, int buflen);

/** Extracts an argument into the given buffer which is sufficiently reallocated.
 *
 * @param buf Command buffer
 * @param buflen Command buffer length
 * @param argbuf Argument buffer
 * @param argbuflen Argument buffer length
 * @param argidx The index of the argument to extract
 * @param addzero Appends a nul-byte to create ASCIIZ string if !0
 * @retval 0 on success
 * @retval -1 on system error (see errno)
 * @retval -2 if the command buffer or argidx are invalid
 */
int ipc_cmd_get_arg(const char* buf, int buflen, char** argbuf, int* argbuflen, int argidx, int addzero);

/** Returns the length of the given argument.
 * @param buf Command buffer
 * @param buflen Command buffer length
 * @param argidx The index of the argument to extract
 * @retval >=0 the requested length
 * @retval -2 on invalid argument index
 */
int ipc_cmd_get_arg_len(const char *buf, int buflen, int argidx);

//get arg and convert to asciiz string. out is newly allocated
int ipc_cmd_get_string_arg(const char* buf, int buflen, int argidx, char** out);

//get arg and convert to int16_t
int ipc_cmd_get_short_arg(const char* buf, int buflen, int argidx, int16_t* out);

//get arg and convert to int32_t
int ipc_cmd_get_long_arg(const char* buf, int buflen, int argidx, int32_t* out);

/** Removes the first complete command from the start of the buffer.
 *
 * @param buf Command buffer
 * @param buflen Command buffer length
 * @retval 1 if a command has been removed
 * @retval 0 if no complete command was present
 * @retval -1 on system error (see errno)
 */
int ipc_cmd_remove(char** buf, int* buflen);

/** Creates a string describing the given command.
 * @param buf Command buffer
 * @param buflen Command buffer length
 * @param is_reply 0 if the command is a request, 1 if it is a reply
 * @param outbuf Output buffer, this will be allocated and nul-terminated
 * @retval 1 if stringification was successful
 * @retval 0 if stringification was not successful (no allocation has been done)
 * @retval -1 on system error (see errno)
 */
int ipc_stringify_cmd(const char *buf, int buflen, int is_reply, char **outbuf);

#ifdef __cplusplus
} //extern "C"
#endif

#endif /* ! IPC_SHARED_H */
