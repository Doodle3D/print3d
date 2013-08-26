#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "print3d.h"

#ifdef __APPLE__
//TODO: check minimum version (tiger)? -> http://mstat.googlecode.com/svn-history/r7/trunk/mstat/SerialPortCommunication.c
#warning "serial not implemented for OSX yet"
//...
#elif __linux
# include <linux/serial.h>
# include <linux/termios.h>
//# include <sys/ioctl.h>
#else
# error "unknown OS, cannot compile serial code"
#endif

static int serial_fd = -1;
//static char* dev_name;
//static int baud_rate;


int serial_open(const char* file) {
	//ESERIAL_SET_SPEED_RESULT spdResult;

	serial_fd = open(file, O_RDWR);
	if (serial_fd < 0) {
		//logger(ERROR, "%s: could not open port %s (%s)\n", argv[0], file, strerror(errno));
		return serial_fd;
	}

	//spdResult = setPortSpeed(serial_fd,baud_rate);

	return 0;
}

int serial_close() {
	if (serial_fd >= 0) return close(serial_fd);

	return 0;
}

#ifdef __linux

typedef enum ESERIAL_SET_SPEED_RESULT {
	SSR_OK = 0, SSR_IO_GET, SSR_IO_SET, SSR_IO_MGET, SSR_IO_MSET1, SSR_IO_MSET2
} SET_SPEED_RESULT;

SET_SPEED_RESULT serial_set_speed(int fd, int speed) {
	struct termios2 options;
	int modemBits;

	if (ioctl(fd, TCGETS2, &options) < 0) return SSR_IO_GET;

	cfmakeraw(&options);

	// Enable the receiver
	options.c_cflag |= CREAD;

	// Clear handshake, parity, stopbits and size
	options.c_cflag &= ~CLOCAL;
	options.c_cflag &= ~CRTSCTS;
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;

	//set speed
	options.c_ospeed = options.c_ispeed = speed;
	options.c_cflag &= ~CBAUD;
	options.c_cflag |= BOTHER;

	options.c_cflag |= CS8;
	options.c_cflag |= CLOCAL;

	if (ioctl(fd, TCSETS2, &options) < 0) return SSR_IO_SET;

	//toggle DTR
	if (ioctl(fd, TIOCMGET, &modemBits) < 0) return SSR_IO_MGET;
	modemBits |= TIOCM_DTR;
	if (ioctl(fd, TIOCMSET, &modemBits) < 0) return SSR_IO_MSET1;
	usleep(100 * 1000);
	modemBits &=~TIOCM_DTR;
	if (ioctl(fd, TIOCMSET, &modemBits) < 0) return SSR_IO_MSET2;

	currentSpeed = speed;

	return SSR_OK;
}

#endif /* __linux */

bool serial_send(const char* code) {
	if (serial_fd >= 0) {
		log_msg(INFO, "serial_send: '%s'", code);
		write(serial_fd, code, strlen(code));
		return TRUE;
	} else {
		return FALSE;
	}
}
