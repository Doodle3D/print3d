#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Serial.h"
#include "../utils.h"

#ifdef __APPLE__

# if (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4) /* require at least 10.4 (OSX Tiger) */
# include <termios.h> /* cfmakeraw */
# include <sys/ioctl.h> /* ioctl */


# include <IOKit/serial/ioss.h>

#  define TERMIOS_TYPE termios
# else
#  error "cannot set serial port speed on OSX versions below 10.4 (Tiger)"
# endif
#elif __linux
# include <linux/serial.h>
# include <linux/termios.h>

#ifdef __cplusplus
extern "C" {
#endif
/** Declarations we need. Using the proper includes creates conflicts with those
 * necessary for termios2.
 */
void cfmakeraw(struct termios2 *termios_p);
int ioctl(int fildes, int request, ... /* arg */);

#ifdef __cplusplus
} //extern "C"
#endif

# define TERMIOS_TYPE termios2
#else
# error "cannot set serial port speed for this OS"
#endif

using std::string;

//NOTE: see Server.cpp for comments on this macro
#define LOG(lvl, fmt, ...) log_.log(lvl, "[SER] " fmt, ##__VA_ARGS__)


const int Serial::READ_BUF_SIZE = 1024;

Serial::Serial()
: portFd_(-1), buffer_(0), bufferSize_(0), log_(Logger::getInstance()) { }

int Serial::open(const char* file) {
	//ESERIAL_SET_SPEED_RESULT spdResult;

  LOG(Logger::VERBOSE,"open(): %s",file);

	portFd_ = ::open(file, O_RDWR | O_NONBLOCK);
	//LOG(Logger::VERBOSE,"  serial opened %i",portFd_);
	if (portFd_ < 0) {
		LOG(Logger::ERROR,"Could not open port %s (%s)\n", file, strerror(errno));
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

bool Serial::isOpen() const {
  return portFd_ > -1;
}

Serial::SET_SPEED_RESULT Serial::setSpeed(int speed) {
	LOG(Logger::VERBOSE,"setSpeed(): %i",speed);
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

  //FIXME: why is CLOCAL disabled above and re-enabled again here?
	options.c_cflag |= CS8;
	options.c_cflag |= CLOCAL;

	//set speed
#ifdef __APPLE__
	//first set speed to 9600, then after tcsetattr set custom speed (as per ofxSerial addon)
  cfsetispeed(&options, B9600);
  cfsetospeed(&options, B9600);

#elif __linux
	options.c_ospeed = options.c_ispeed = speed;
	options.c_cflag &= ~CBAUD;
	options.c_cflag |= BOTHER;
#endif

#ifdef __APPLE__
	if (tcsetattr(portFd_, TCSANOW, &options) < 0) return SSR_IO_SET;
	if (ioctl(portFd_, IOSSIOSPEED, &speed) == -1) return SSR_IO_IOSSIOSPEED;
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
  //LOG(Logger::VERBOSE,"Serial::send(): %s",code);
	if (portFd_ >= 0) {
		::write(portFd_, code, strlen(code));
		return true;
	} else {
		return false;
	}
}

int Serial::readData() {
  //Logger& log = Logger::getInstance();
	//log.log(Logger::VERBOSE,"Serial::readData");


  int rv = readAndAppendAvailableData(portFd_, &buffer_, &bufferSize_, 0, 0);
  return rv;
}

char* Serial::getBuffer() {
  return buffer_;
}

int Serial::getBufferSize() const {
  return bufferSize_;
}

int Serial::getFileDescriptor() const {
	return portFd_;
}

string* Serial::extractLine() {
  char* p = buffer_;
  while(p < buffer_ + bufferSize_) {
    if (*p == '\n') break;
    p++;
  }
  if (p == buffer_ + bufferSize_) return NULL;

  int lineLen = p - buffer_;
  char* lineCopy = (char*)malloc(lineLen);
  memcpy(lineCopy, buffer_, lineLen);

  //this is rather ugly but at least it should work...
  lineCopy[lineLen] = '\0'; //overwrite \n with nulbyte
  if (lineCopy[lineLen - 1] == '\r') lineCopy[lineLen - 1] = '\0'; //also remove \r if present
  string* line = new string(lineCopy);
  free(lineCopy);

  //now resize the read buffer
  memmove(buffer_, p + 1, bufferSize_ - (lineLen + 1));
  bufferSize_ -= lineLen + 1;
  buffer_ = (char*)realloc(buffer_, bufferSize_);

  return line;
}
