#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "Serial.h"

#include <termios.h> /* cfmakeraw */
#include <sys/ioctl.h> /* ioctl */

#ifdef __APPLE__
# if (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4) /* require at least 10.4 (OSX Tiger) */
#  include <IOKit/serial/ioss.h>

#  define TERMIOS_TYPE termios
# else
#  error "cannot set serial port speed on OSX versions below 10.4 (Tiger)"
# endif
#elif __linux
# include <linux/serial.h>
# include <linux/termios.h>

# define TERMIOS_TYPE termios2
#else
# error "cannot set serial port speed for this OS"
#endif


Serial::Serial()
: portFd_(0)
{ /* empty */ }

int Serial::open(const char* file) {
	//ESERIAL_SET_SPEED_RESULT spdResult;

	portFd_ = ::open(file, O_RDWR);
	if (portFd_ < 0) {
		//logger(ERROR, "%s: could not open port %s (%s)\n", argv[0], file, strerror(errno));
		return portFd_;
	}

	//spdResult = setPortSpeed(serial_fd,baud_rate);

	return 0;
}

int Serial::close() {
	if (portFd_ >= 0) return ::close(portFd_);

	return 0;
}

Serial::SET_SPEED_RESULT Serial::setSpeed(int fd, int speed) {
	struct TERMIOS_TYPE options;
	int modemBits;

#ifdef __APPLE__
	if (tcgetattr(fd, &options) < 0) return SSR_IO_GET;
#else
	if (ioctl(fd, TCGETS2, &options) < 0) return SSR_IO_GET;
#endif

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
#ifdef __APPLE__
	if (ioctl(portFd_, IOSSIOSPEED, &speed) == -1) return SSR_IO_IOSSIOSPEED;
#elif __linux
	options.c_ospeed = options.c_ispeed = speed;
	options.c_cflag &= ~CBAUD;
	options.c_cflag |= BOTHER;
#endif

	options.c_cflag |= CS8;
	options.c_cflag |= CLOCAL;

#ifdef __APPLE__
	if (tcsetattr(fd, TCSANOW, &options) < 0) return SSR_IO_SET;
#else
	if (ioctl(fd, TCSETS2, &options) < 0) return SSR_IO_SET;
#endif

	//toggle DTR
	if (ioctl(fd, TIOCMGET, &modemBits) < 0) return SSR_IO_MGET;
	modemBits |= TIOCM_DTR;
	if (ioctl(fd, TIOCMSET, &modemBits) < 0) return SSR_IO_MSET1;
	usleep(100 * 1000);
	modemBits &=~TIOCM_DTR;
	if (ioctl(fd, TIOCMSET, &modemBits) < 0) return SSR_IO_MSET2;

	return SSR_OK;
}

bool Serial::send(const char* code) const {
	if (portFd_ >= 0) {
		//log_msg(INFO, "serial_send: '%s'", code);
		::write(portFd_, code, strlen(code));
		return true;
	} else {
		return false;
	}
}
