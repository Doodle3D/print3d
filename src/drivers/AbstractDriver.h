#ifndef ABSTRACT_DRIVER_H_SEEN
#define ABSTRACT_DRIVER_H_SEEN

#include <inttypes.h>
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
  typedef AbstractDriver* (*creatorFunc)(const std::string& serialPortPath, const uint32_t& baudrate);
  // driver info per driver (used in DriverFactory)
  struct DriverInfo {
	  vec_FirmwareDescription supportedFirmware;
	  creatorFunc creator;                                //pointer to driver's create function
  };

  // drivers should provide a function which returns a reference to a DriverInfo object
  //   static const AbstractDriver::DriverInfo& getDriverInfo();

  typedef enum STATE{
  	DISCONNECTED,
  	IDLE,
  	PRINTING /* executing commands */
  } STATE;

  explicit AbstractDriver(const std::string& serialPortPath, const uint32_t& baudrate);
  virtual ~AbstractDriver();

  int openConnection();
  int closeConnection();

  // should return in how much milliseconds it wants to be called again
  virtual int update() = 0;





  void setGCode(const std::string& gcode);
  void appendGCode(const std::string& gcode);
  void clearGCode();

  void startPrint(const std::string& gcode);
  void startPrint();
  void stopPrint();

  /*
	 * Get cached extruder temperature
	 */
	int getTemperature() const;
	/*
	 * Get cached bed temperature
	 */
	int getBedTemperature() const;
	/*
	 * Get line number of codeBuffer that the printer should be printing
	 */
  int getCurrentLine() const;
  /*
	 * Get line number of codeBuffer that the printer should be printing
	 */
  int getNumLines() const;

  STATE getState();
  std::string getStateString(STATE state);

protected:
  int temperature_;
  int bedTemperature_;
  int currentLine_;
  int numLines_;
  STATE state_;

  std::string gcodeBuffer;

  void printNextLine();
  void resetPrint();

  void checkTemperature();

  virtual void sendCode(const std::string& code) = 0;
  virtual void readCode(std::string& code) = 0;

  /*
   * Convenience function to extract (and remove) line from codeBuffer
   */
  bool extractLine(std::string& line);

  virtual void updateGCodeInfo();

  void setState(STATE state);

  int readData();
  void setBaudrate(uint32_t baudrate);
  void switchBaudrate();
  Serial serial_;
  Logger& log_;

private:
	const std::string serialPortPath_;
	uint32_t baudrate_;

	typedef enum BAUDRATE{
		B115200 = 115200,
		B250000 = 250000
	} BAUDRATE;
};

#endif /* ! ABSTRACT_DRIVER_H_SEEN */
