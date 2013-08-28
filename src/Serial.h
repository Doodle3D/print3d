#ifndef SERIAL_H_SEEN
#define SERIAL_H_SEEN

class Serial {
public:
	typedef enum ESERIAL_SET_SPEED_RESULT {
		SSR_OK = 0, SSR_IO_GET, SSR_IO_IOSSIOSPEED, SSR_IO_SET, SSR_IO_MGET, SSR_IO_MSET1, SSR_IO_MSET2
	} SET_SPEED_RESULT;

	Serial();

	int open(const char* file);
	int close();
	SET_SPEED_RESULT setSpeed(int fd, int speed);
	bool send(const char* code) const;

private:
	int portFd_;
	//static char* dev_name;
	//static int baud_rate;
};

#endif /* SERIAL_H_SEEN */
