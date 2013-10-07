#include <errno.h>
#include <algorithm>
#include <sstream>
#include "AbstractDriver.h"
#include "../server/Server.h"

using std::string;
using std::size_t;

//NOTE: see Server.cpp for comments on this macro
#define LOG(lvl, fmt, ...) log_.log(lvl, "[ABD] " fmt, ##__VA_ARGS__)


//STATIC
const string AbstractDriver::STATE_NAMES[] = { "unknown", "disconnected", "idle", "printing", "stopping" };

AbstractDriver::AbstractDriver(Server& server, const string& serialPortPath, const uint32_t& baudrate)
: temperature_(0),
  targetTemperature_(0),
  bedTemperature_(0),
  targetBedTemperature_(0),
  state_(DISCONNECTED),
  log_(Logger::getInstance()),
  server_(server),
  serialPortPath_(serialPortPath),
  baudrate_(baudrate) { }

AbstractDriver::~AbstractDriver() {
	serial_.close();
}


int AbstractDriver::openConnection() {
	LOG(Logger::VERBOSE,"openConnection()");
	LOG(Logger::VERBOSE,"  serial port path: '%s', baudrate: %i",serialPortPath_.c_str(), baudrate_);
	int rv = serial_.open(serialPortPath_.c_str());
	//LOG(Logger::BULK,"  serial opened (%i)",rv);
	if (rv < 0) {
		return rv;
	}
	// TODO: when this baudrate is incorrect another should be tried
	//  and it should be reported so that the one that does works gets
	//  saved as preference.
	server_.registerFileDescriptor(serial_.getFileDescriptor());
	setBaudrate(baudrate_);

	return 0;
}

int AbstractDriver::closeConnection() {
	setState(DISCONNECTED);
	server_.unregisterFileDescriptor(serial_.getFileDescriptor());
	return serial_.close();
}

bool AbstractDriver::isConnected() {
	return serial_.isOpen();
}


/*************************************
 ******** Manage GCode buffer ********
 ************************************/
/*
 * Set (replace) the GCode buffer
 */
void AbstractDriver::setGCode(const std::string& gcode) {
	gcodeBuffer_.set(gcode);
}
/*
 * Append GCode to GCode buffer
 */
void AbstractDriver::appendGCode(const std::string& gcode) {
	gcodeBuffer_.append(gcode);
}

/*
 * Clear (empty) GCode buffer
 */
void AbstractDriver::clearGCode() {
	gcodeBuffer_.clear();
}

void AbstractDriver::heatup(int temperature) {
	std::ostringstream oss;
	oss << "M104 S" << temperature;
	sendCode(oss.str());
}

/*
 * Print control
 */
void AbstractDriver::startPrint(const std::string& gcode, STATE state) {
	LOG(Logger::BULK, "startPrint(): %s",gcode.c_str());
	setGCode(gcode);
	startPrint(state);
}

void AbstractDriver::startPrint(STATE state) {
	LOG(Logger::BULK, "startPrint");
	resetPrint();
	setState(state);
	printNextLine();
}

void AbstractDriver::stopPrint() {
	stopPrint("");
}

void AbstractDriver::stopPrint(const std::string& endcode) {
	LOG(Logger::BULK, "stopPrint(): with %i bytes of end g-code", endcode.length());
	resetPrint();
	setGCode(endcode);
	startPrint(STOPPING);
}

void AbstractDriver::resetPrint() {
	LOG(Logger::BULK, "resetPrint()");
	setState(IDLE);
	gcodeBuffer_.setCurrentLine(0);
}

void AbstractDriver::printNextLine() {
	LOG(Logger::BULK, "printNextLine(): %i/%i",gcodeBuffer_.getCurrentLine(), gcodeBuffer_.getTotalLines());
	string line;
	if(gcodeBuffer_.getNextLine(line)) {
		sendCode(line);
		gcodeBuffer_.setCurrentLine(gcodeBuffer_.getCurrentLine() + 1);
	} else { // print finished
		resetPrint();
	}
}


/*
 * Getters and setters
 */
int AbstractDriver::getTemperature() const {
	return temperature_;
}

int AbstractDriver::getTargetTemperature() const {
	return targetTemperature_;
}

int AbstractDriver::getBedTemperature() const {
	return bedTemperature_;
}

int AbstractDriver::getTargetBedTemperature() const {
	return targetBedTemperature_;
}

int AbstractDriver::getCurrentLine() const {
	return gcodeBuffer_.getCurrentLine();
}

int AbstractDriver::getNumLines() const {
	return gcodeBuffer_.getTotalLines();
}

AbstractDriver::STATE AbstractDriver::getState() {
	return state_;
}
void AbstractDriver::setState(STATE state) {
	LOG(Logger::BULK, "setState(): %i:%s > %i:%s",state_,getStateString(state_).c_str(),state,getStateString(state).c_str());
	state_ = state;
}

const std::string &AbstractDriver::getStateString(STATE state) {
	return STATE_NAMES[state];
}

int AbstractDriver::readData() {

	//LOG(Logger::BULK, "readData()");

	int rv = serial_.readData();
	log_.checkError(rv, "cannot read from device");
	if (rv == -2) {
		LOG(Logger::ERROR, "remote end closed connection, closing port");
		serial_.close();
	} else if (rv == -1 && errno == ENXIO) {
		LOG(Logger::ERROR, "port was disconnected, closing port");
		serial_.close();
	} else if (rv >= 0) {
		//LOG(Logger::BULK, "read %i bytes from device", rv);
	}
	return rv;
}

void AbstractDriver::setBaudrate(uint32_t baudrate) {
	baudrate_ = baudrate;
	Serial::ESERIAL_SET_SPEED_RESULT ssr = serial_.setSpeed(baudrate_);
	if(ssr == Serial::SSR_OK) {
		setState(IDLE);
	} else {
		LOG(Logger::ERROR,"  setting speed error");
	}
}

//TODO: add 57600?
void AbstractDriver::switchBaudrate() {
	setBaudrate((baudrate_ == B250000)? B115200 : B250000);
}
