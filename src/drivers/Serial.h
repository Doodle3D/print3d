#ifndef SERIAL_H_SEEN
#define SERIAL_H_SEEN

#include <string>
#include "../server/Logger.h"

class Serial {
public:
	typedef enum ESERIAL_SET_SPEED_RESULT {
		SSR_OK = 0, SSR_IO_GET, SSR_IO_IOSSIOSPEED, SSR_IO_SET, SSR_IO_MGET, SSR_IO_MSET1, SSR_IO_MSET2
	} SET_SPEED_RESULT;

	Serial();

	int open(const char* file);
	int close();
  bool isOpen();
	SET_SPEED_RESULT setSpeed(int speed);
	bool send(const char* code) const;

  int readData();

  char* getBuffer();
  int getBufferSize();

  //convenience function for plain text data
  std::string* extractLine();

private:
  static const int READ_BUF_SIZE;

	Serial(const Serial& o);
	void operator=(const Serial& o);

	int portFd_;

  char* buffer_;
  int bufferSize_;

	//static char* dev_name;
	//static int baud_rate;
};

#endif /* SERIAL_H_SEEN */
