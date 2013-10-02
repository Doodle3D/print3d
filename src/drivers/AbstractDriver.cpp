#include <errno.h>
#include <algorithm>
#include <sstream>
#include "AbstractDriver.h"
#include "../server/Server.h"

using std::string;
using std::size_t;

//STATIC
const string AbstractDriver::STATE_NAMES[] = { "unknown", "disconnected", "idle", "printing", "stopping" };

AbstractDriver::AbstractDriver(Server& server, const string& serialPortPath, const uint32_t& baudrate)
: temperature_(0),
  targetTemperature_(0),
  bedTemperature_(0),
  targetBedTemperature_(0),
  currentLine_(1),
  numLines_(-1),
  state_(DISCONNECTED),
  log_(Logger::getInstance()),
  server_(server),
  serialPortPath_(serialPortPath),
  baudrate_(baudrate) { }

AbstractDriver::~AbstractDriver() {
	serial_.close();
}


int AbstractDriver::openConnection() {
	log_.log(Logger::VERBOSE,"AbstractDriver::openConnection");
	log_.log(Logger::VERBOSE,"  serial port path: '%s', baudrate: %i",serialPortPath_.c_str(), baudrate_);
	int rv = serial_.open(serialPortPath_.c_str());
	//log_.log(Logger::VERBOSE,"  serial opened (%i)",rv);
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
	gcodeBuffer_ = gcode;
	filterGCode();
	updateGCodeInfo();
}
/*
 * Append GCode to GCode buffer
 */
void AbstractDriver::appendGCode(const std::string& gcode) {
	setGCode(gcodeBuffer_ + gcode);
}

/*
 * Clear (empty) GCode buffer
 */
void AbstractDriver::clearGCode() {
	setGCode("");
}

/*
 * Filter GCode, remove comments for example
 */
void AbstractDriver::filterGCode() {
	// replace \r with \n
	std::replace( gcodeBuffer_.begin(), gcodeBuffer_.end(), '\r', '\n');

	// replace \n\n with \n
	std::size_t posDoubleNewline = 0;
	while((posDoubleNewline = gcodeBuffer_.find("\n\n")) != string::npos) {
		gcodeBuffer_.replace(posDoubleNewline, 2, "\n");
	}

	// remove all comments (;...)
	std::size_t posComment = 0;
	while((posComment = gcodeBuffer_.find(';')) != string::npos) {
		std::size_t posCommentEnd = gcodeBuffer_.find('\n', posComment);
		if(posCommentEnd == string::npos) { // no newline found
			gcodeBuffer_.erase(posComment); // so erase remaining text
		} else {
			gcodeBuffer_.erase(posComment,posCommentEnd - posComment);
		}
	}
}

/*
 * Update GCode info, like number of lines.
 */
void AbstractDriver::updateGCodeInfo() {
	numLines_ = std::count(gcodeBuffer_.begin(), gcodeBuffer_.end(), '\n') + 1;
	currentLine_ = std::min(currentLine_,numLines_-1);
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
	log_.log(Logger::BULK, "AbstractDriver::startPrint: %s",gcode.c_str());
	setGCode(gcode);
	startPrint(state);
}

void AbstractDriver::startPrint(STATE state) {
	log_.log(Logger::BULK, "AbstractDriver::startPrint");
	resetPrint();
	setState(state);
	printNextLine();
}

void AbstractDriver::stopPrint() {
	resetPrint();
}

void AbstractDriver::stopPrint(const std::string& endcode) {
	log_.log(Logger::BULK, "AbstractDriver::stopPrint: with %i bytes of end g-code", endcode.length());
	resetPrint();
	setGCode(endcode);
	startPrint(STOPPING);
}

void AbstractDriver::resetPrint() {
	log_.log(Logger::BULK, "AbstractDriver::resetPrint");
	setState(IDLE);
	currentLine_ = 1;
}

void AbstractDriver::printNextLine() {
	log_.log(Logger::BULK, "AbstractDriver::printNextLine: %i/%i",currentLine_,numLines_);
	if(currentLine_ <= numLines_) {
		string line;
		if(getNextLine(line)) {
			sendCode(line);
			currentLine_++;
		}
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
	return currentLine_;
}

int AbstractDriver::getNumLines() const {
	return numLines_;
}

AbstractDriver::STATE AbstractDriver::getState() {
	return state_;
}
void AbstractDriver::setState(STATE state) {
	log_.log(Logger::BULK, "AbstractDriver::setState: %i:%s > %i:%s",state_,getStateString(state_).c_str(),state,getStateString(state).c_str());
	state_ = state;
}

const std::string &AbstractDriver::getStateString(STATE state) {
	return STATE_NAMES[state];
}

int AbstractDriver::readData() {

	//log_.log(Logger::BULK, "AbstractDriver::readData");

	int rv = serial_.readData();
	log_.checkError(rv, "cannot read from device");
	if (rv == -2) {
		log_.log(Logger::ERROR, "remote end closed connection, closing port");
		serial_.close();
	} else if (rv == -1 && errno == ENXIO) {
		log_.log(Logger::ERROR, "port was disconnected, closing port");
		serial_.close();
	} else if (rv >= 0) {
		//log_.log(Logger::BULK, "read %i bytes from device", rv);
	}
	return rv;
}

void AbstractDriver::setBaudrate(uint32_t baudrate) {
	baudrate_ = baudrate;
	Serial::ESERIAL_SET_SPEED_RESULT ssr = serial_.setSpeed(baudrate_);
	if(ssr == Serial::SSR_OK) {
		setState(IDLE);
	} else {
		log_.log(Logger::ERROR,"  setting speed error");
	}
}

//TODO: add 57600?
void AbstractDriver::switchBaudrate() {
	setBaudrate((baudrate_ == B250000)? B115200 : B250000);
}

bool AbstractDriver::getNextLine(std::string& line) {
	size_t posN = gcodeBuffer_.find("\n");
	if(posN == string::npos) {
		line = gcodeBuffer_.substr(0); // extract all
		if(line.length() > 0) return true;
		else return false;
	} else {
		line = gcodeBuffer_.substr(0,posN);
		return true;
	}
}

bool AbstractDriver::erasePrevLine() {
	size_t posN = gcodeBuffer_.find("\n");
	if(posN == string::npos) {
		//NOTE: use clear() instead of erase(0) to prevent a compiler error in openwrt
		gcodeBuffer_.clear();
		return false;
	} else {
		gcodeBuffer_.erase(0,posN+1);
		return true;
	}
}
