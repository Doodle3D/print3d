#ifndef ABSTRACT_DRIVER_H_SEEN
#define ABSTRACT_DRIVER_H_SEEN

#include <string>
#include <vector>
#include "Serial.h"

class AbstractDriver {
public:

  // description of firmware a driver supports. TODO: add human readable names
	struct FirmwareDescription {
		std::string name;
    FirmwareDescription(const std::string& n)
    : name(n)
    {}
	};
  // typedef (shorthand) for list of firmware descriptions
  typedef std::vector<FirmwareDescription> vec_FirmwareDescription;
  // typedef (shorthand) for create instance function of driver
  typedef AbstractDriver* (*creatorFunc)(const std::string& serialPortPath);
  // driver info per driver (used in DriverFactory)
  struct DriverInfo {
		vec_FirmwareDescription supportedFirmware;
    creatorFunc creator;                                //pointer to driver's create function
	};

  // drivers should provide a function which returns a reference to a DriverInfo object
  //   static const AbstractDriver::DriverInfo& getDriverInfo();



	explicit AbstractDriver(const std::string& serialPortPath);
	virtual ~AbstractDriver();

	int openConnection();
	int closeConnection();

	int getTemperature() const;

  int getCurrentLine() const;
  int getNumLines() const;

  void printGCode(std::string gcode);
  void stopPrint();

  // should return in how much milliseconds it wants to be called again
	virtual int update() = 0;

protected:
  int temperature_;
	int currentLine_;
	int numLines_;

  std::string codeBuffer;

  void checkTemperature();

  virtual void sendCode(const std::string& code) = 0;
  virtual void readCode(std::string& code) = 0;

  int readData();
	Serial serial_;

private:
	const std::string serialPortPath_;


};

#endif /* ! ABSTRACT_DRIVER_H_SEEN */
