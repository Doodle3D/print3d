#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "Serial.h"

using std::string;

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


const int Serial::READ_BUF_SIZE = 1024;

Serial::Serial()
: portFd_(-1) {
}

int Serial::open(const char* file) {
	//ESERIAL_SET_SPEED_RESULT spdResult;

  Logger& log = Logger::getInstance();
	log.log(Logger::VERBOSE,"Serial::open");
  log.log(Logger::VERBOSE,"  file: %s",file);

	portFd_ = ::open(file, O_RDWR | O_NONBLOCK);
	log.log(Logger::VERBOSE,"  serial opened %i",portFd_);
	if (portFd_ < 0) {
    Logger& log = Logger::getInstance();
  	log.log(Logger::ERROR,"Could not open port %s\n", file);
		return portFd_;
	}

	//spdResult = setPortSpeed(serial_fd,baud_rate);

	return 0;
}

int Serial::close() {
  int rv = 0;

	if (portFd_ >= 0) {
    rv = ::close(portFd_);
    portFd_ = -1;
  }

	return rv;
}

bool Serial::isOpen() {
  return portFd_ > -1;
}

Serial::SET_SPEED_RESULT Serial::setSpeed(int speed) {
	struct TERMIOS_TYPE options;
	int modemBits;

#ifdef __APPLE__
	if (tcgetattr(portFd_, &options) < 0) return SSR_IO_GET;
#else
	if (ioctl(portFd_, TCGETS2, &options) < 0) return SSR_IO_GET;
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
	if (tcsetattr(portFd_, TCSANOW, &options) < 0) return SSR_IO_SET;
#else
	if (ioctl(portFd_, TCSETS2, &options) < 0) return SSR_IO_SET;
#endif

	//toggle DTR
	if (ioctl(portFd_, TIOCMGET, &modemBits) < 0) return SSR_IO_MGET;
	modemBits |= TIOCM_DTR;
	if (ioctl(portFd_, TIOCMSET, &modemBits) < 0) return SSR_IO_MSET1;
	usleep(100 * 1000);
	modemBits &=~TIOCM_DTR;
	if (ioctl(portFd_, TIOCMSET, &modemBits) < 0) return SSR_IO_MSET2;

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

int Serial::readData() {
  int newSize = bufferSize_ + READ_BUF_SIZE;
  buffer_ = (char*)realloc(buffer_, newSize);

  int rv = read(portFd_, buffer_ + bufferSize_, newSize - bufferSize_);
  int readLen;

  if (rv < 0) {
    //EINTR -> restart
    //EWOULDBLOCK / EAGAIN -> would block in case of non-blocking fd
    readLen = 0;
  } else if (rv == 0) {
    close();
    readLen = 0;
  } else {
    readLen = rv;
  }

  bufferSize_ += readLen;
  buffer_ = (char*)realloc(buffer_, bufferSize_);

  return rv;
}

char* Serial::getBuffer() {
  return buffer_;
}

int Serial::getBufferSize() {
  return bufferSize_;
}

string* Serial::extractLine() {
  char* p = strchr(buffer_, '\n');
  if (!p) return NULL;

  int lineLen = p - buffer_;
  char* lineCopy = (char*)malloc(lineLen + 1);
  memcpy(lineCopy, buffer_, lineLen);

  //this is rather ugly but at least it should work...
  lineCopy[lineLen] = '\0';
  string* line = new string(lineCopy);
  free(lineCopy);

  //now resize the read buffer
  memmove(buffer_, p + 1, bufferSize_ - lineLen - 1);
  bufferSize_ -= lineLen - 1;
  buffer_ = (char*)realloc(buffer_, bufferSize_);

  return line;
}
