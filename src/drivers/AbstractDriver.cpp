#include "AbstractDriver.h"
#include <algorithm>

using std::string;
using std::size_t;

AbstractDriver::AbstractDriver(const string& serialPortPath, const uint32_t& baudrate)
: temperature_(0),
  targetTemperature_(0),
  bedTemperature_(0),
  targetBedTemperature_(0),
  currentLine_(1),
  numLines_(-1),
  state_(DISCONNECTED),
  log_(Logger::getInstance()),
  serialPortPath_(serialPortPath),
  baudrate_(baudrate) { }

AbstractDriver::~AbstractDriver() {
	serial_.close();
}

int AbstractDriver::openConnection() {
	log_.log(Logger::VERBOSE,"AbstractDriver::openConnection");
	log_.log(Logger::VERBOSE,"  serialPortPath_: %s",serialPortPath_.c_str());
	log_.log(Logger::VERBOSE,"  baudrate_: %i",baudrate_);
	int rv = serial_.open(serialPortPath_.c_str());
	//log_.log(Logger::VERBOSE,"  serial opened (%i)",rv);
	if (rv < 0) {
		return rv;
	}
	// TODO: when this baudrate is incorrect another should be tried
	//  and it should be reported so that the one that does works gets
	//  saved as preference.
	setBaudrate(baudrate_);

	return 0;
}

int AbstractDriver::closeConnection() {
	setState(DISCONNECTED);
	return serial_.close();
}
/*************************************
 ******** Manage GCode buffer ********
 ************************************/
/*
 * Set (replace) the GCode buffer
 */
void AbstractDriver::setGCode(const std::string& gcode) {
	gcodeBuffer = gcode;
	filterGCode();
	updateGCodeInfo();
}
/*
 * Append GCode to GCode buffer
 */
void AbstractDriver::appendGCode(const std::string& gcode) {
	setGCode(gcodeBuffer + gcode);
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
	// replace \r for \n
	std::replace( gcodeBuffer.begin(), gcodeBuffer.end(), '\r', '\n');

	// replace \n\n for \n
	std::size_t posDoubleNewline = 0;
	while((posDoubleNewline = gcodeBuffer.find("\n\n")) != string::npos) {
		gcodeBuffer.replace(posDoubleNewline, 2, "\n");
	}

	// remove all comments (;...)
	std::size_t posComment = 0;
	while((posComment = gcodeBuffer.find(';')) != string::npos) {
		std::size_t posCommentEnd = gcodeBuffer.find('\n',posComment);
		if(posCommentEnd == string::npos) { // no newline found
			gcodeBuffer.erase(posComment); // so erase remaining text
		} else {
			gcodeBuffer.erase(posComment,posCommentEnd-posComment+1);
		}
	}
}
/*
 * Update GCode info, like number of lines.
 */
void AbstractDriver::updateGCodeInfo() {
	numLines_ = std::count(gcodeBuffer.begin(), gcodeBuffer.end(), '\n') + 1;
	currentLine_ = std::min(currentLine_,numLines_-1);
}

/*
 * Print control
 */
void AbstractDriver::startPrint(const std::string& gcode) {
	log_.log(Logger::BULK, "AbstractDriver::startPrint: %s",gcode.c_str());
	setGCode(gcode);
	startPrint();
}

void AbstractDriver::startPrint() {
	log_.log(Logger::BULK, "AbstractDriver::startPrint");
	resetPrint();
	setState(PRINTING);
	printNextLine();
}

void AbstractDriver::stopPrint() {
	resetPrint();
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

int AbstractDriver::getBedTemperature() const {
	return bedTemperature_;
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

std::string AbstractDriver::getStateString(STATE state) {
	static const char *stateString[] = {"disconnected","idle","printing"};
	return stateString[state];
}
int AbstractDriver::readData() {

	//log_.log(Logger::BULK, "AbstractDriver::readData");

	int rv = serial_.readData();
	log_.checkError(rv, "cannot read from device");
	if (rv == -2) {
		log_.log(Logger::INFO, "nothing to read anymore (remote end closed?)");
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
void AbstractDriver::switchBaudrate() {
	setBaudrate((baudrate_ == B250000)? B115200 : B250000);
}
bool AbstractDriver::getNextLine(std::string& line) {
	size_t posN = gcodeBuffer.find("\n");
	if(posN == string::npos) {
		line = gcodeBuffer.substr(0); // substract all
		if(line.length() > 0) return true;
		else return false;
	} else {
		line = gcodeBuffer.substr(0,posN);
		return true;
	}
}
bool AbstractDriver::erasePrevLine() {
	size_t posN = gcodeBuffer.find("\n");
	if(posN == string::npos) {
		gcodeBuffer.erase(0);
		return false;
	} else {
		gcodeBuffer.erase(0,posN+1);
		return true;
	}
}
