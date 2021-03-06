/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <errno.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include "AbstractDriver.h"
#include "../server/Server.h"

using std::string;
using std::size_t;

//NOTE: see Server.cpp for comments on this macro
#define LOG(lvl, fmt, ...) log_.log(lvl, "ABSD", fmt, ##__VA_ARGS__)

//STATIC
//Note: the state names are used all the way on the other end in javascript, consider this when changing them.
const string AbstractDriver::STATE_NAMES[] = { "unknown", "disconnected", "connecting", "idle", "buffering", "printing", "stopping" };
const bool AbstractDriver::REQUEST_EXIT_ON_PORT_FAIL = true;
const int AbstractDriver::VERBOSE_LOG_NEXT_LINE_EVERY_N_LINES = 25;

AbstractDriver::AbstractDriver(Server& server, const string& serialPortPath, const uint32_t& baudrate)
: heating_(false),
  temperature_(0),
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
	LOG(Logger::INFO,"openConnection()");
	LOG(Logger::INFO,"  serial port path: '%s', baudrate: %i",serialPortPath_.c_str(), baudrate_);
	int rv = serial_.open(serialPortPath_.c_str());
	//LOG(Logger::VERBOSE,"  serial opened (%i)",rv);
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

bool AbstractDriver::isConnected() const {
	return serial_.isOpen();
}


/*************************************
 ******** Manage GCode buffer ********
 ************************************/
/*
 * Set (replace) the GCode buffer
 */
GCodeBuffer::GCODE_SET_RESULT AbstractDriver::setGCode(const std::string& gcode, int32_t totalLines, GCodeBuffer::MetaData *metaData) {
	GCodeBuffer::GCODE_SET_RESULT gsr = gcodeBuffer_.set(gcode, totalLines, metaData);

	if (gsr == GCodeBuffer::GSR_OK) {
		extractGCodeInfo(gcode);
		if (getState() == IDLE) setState(BUFFERING);
	}

	return gsr;
}
/*
 * Append GCode to GCode buffer
 */
GCodeBuffer::GCODE_SET_RESULT AbstractDriver::appendGCode(const std::string& gcode, int32_t totalLines, GCodeBuffer::MetaData *metaData) {
	GCodeBuffer::GCODE_SET_RESULT gsr = gcodeBuffer_.append(gcode, totalLines, metaData);
	if (gsr == GCodeBuffer::GSR_OK) {
		extractGCodeInfo(gcode);
		if (getState() == IDLE) setState(BUFFERING);
	}
	return gsr;
}

/*
 * Clear (empty) GCode buffer
 */
void AbstractDriver::clearGCode() {
	gcodeBuffer_.clear();
	STATE s = getState();
	if (s == BUFFERING || s == PRINTING || s == STOPPING) setState(IDLE);
}

void AbstractDriver::heatup(int temperature) {
	std::ostringstream oss;
	oss << "M104 S" << temperature;
	sendCode(oss.str());
}

/*
 * Print control
 */
bool AbstractDriver::startPrint(const std::string& gcode, STATE state) {
	LOG(Logger::VERBOSE, "startPrint(): %s",gcode.c_str());
	if (gcode.length() > 0) setGCode(gcode);
	return startPrint(state);
}

bool AbstractDriver::stopPrint(const std::string& endcode) {
	LOG(Logger::VERBOSE, "stopPrint(): with %i bytes of end g-code", endcode.length());
	resetPrint();
	setGCode(endcode);
	return startPrint(STOPPING);
}

bool AbstractDriver::startPrint(STATE state) {
	STATE s = getState();
	if (!isPrinterOnline()) {
		LOG(Logger::VERBOSE, "startPrint: printer not online (state==%s)", getStateString(s).c_str());
		return false;
	}

	LOG(Logger::VERBOSE, "startPrint");
	if (s != PRINTING && s != STOPPING) resetPrint();
	setState(state);
	//printNextLine();
	return true;
}

bool AbstractDriver::stopPrint() {
	return stopPrint("");
}


/*
 * Getters and setters
 */

/*
 * Get cached heating state
 */
bool AbstractDriver::isHeating() const {
	return heating_;
}

/*
 * Get cached extruder temperature
 */
uint16_t AbstractDriver::getTemperature() const {
	return temperature_;
}

/*
 * Get cached extruder target temperature
 */
uint16_t AbstractDriver::getTargetTemperature() const {
	return targetTemperature_;
}

/*
 * Get cached bed temperature
 */
uint16_t AbstractDriver::getBedTemperature() const {
	return bedTemperature_;
}

/*
 * Get cached bed target temperature
 */
uint16_t AbstractDriver::getTargetBedTemperature() const {
	return targetBedTemperature_;
}

/*
 * Get the line number which is currently being printed (current/total*100 yields progress percentage)
 */
int32_t AbstractDriver::getCurrentLine() const {
	return gcodeBuffer_.getCurrentLine();
}

/*
 * Get the number of lines currently in the buffer
 */
int32_t AbstractDriver::getBufferedLines() const {
	return gcodeBuffer_.getBufferedLines();
}

/*
 * Get the total number of lines which were appended to the buffer after the last clear or set
 */
int32_t AbstractDriver::getTotalLines() const {
	return gcodeBuffer_.getTotalLines();
}

/*
 * Get the current buffer size in bytes, useful in combination with the maximum size to inspect how full the buffer is.
 */
int32_t AbstractDriver::getBufferSize() const {
	return gcodeBuffer_.getBufferSize();
}

/*
 * Get the current maximum buffer size in bytes, note that this may be a dynamic value
 */
int32_t AbstractDriver::getMaxBufferSize() const {
	return gcodeBuffer_.getMaxBufferSize();
}

/*
 * Get metadata from the GCode buffer.
 */
const GCodeBuffer::MetaData* AbstractDriver::getMetaData() const {
	return gcodeBuffer_.getMetaData();
}

AbstractDriver::STATE AbstractDriver::getState() const {
	return state_;
}

const std::string &AbstractDriver::getStateString(STATE state) {
	return STATE_NAMES[state];
}


/***********************
 * PROTECTED FUNCTIONS *
 ***********************/

void AbstractDriver::printNextLine() {
	int32_t curLine = gcodeBuffer_.getCurrentLine();
	if (log_.getLevel() == Logger::VERBOSE && curLine % VERBOSE_LOG_NEXT_LINE_EVERY_N_LINES == 0) {
		LOG(Logger::VERBOSE, "printNextLine(): %i/%i (logging only every %i lines)",
				curLine, gcodeBuffer_.getTotalLines(), VERBOSE_LOG_NEXT_LINE_EVERY_N_LINES);
	} else {
		LOG(Logger::BULK, "printNextLine(): %i/%i", curLine, gcodeBuffer_.getTotalLines());
	}

	string line;
	if(gcodeBuffer_.getNextLine(line) > 0) {
		sendCode(line);
		gcodeBuffer_.setCurrentLine(gcodeBuffer_.getCurrentLine() + 1);
	} else { // print finished
		resetPrint();
	}
}

bool AbstractDriver::resetPrint() {
	if (!isPrinterOnline()) {
		LOG(Logger::VERBOSE, "resetPrint: printer not online (state==%s)", getStateString(getState()).c_str());
		return false;
	}

	LOG(Logger::VERBOSE, "resetPrint()");
	setState(IDLE);
	gcodeBuffer_.setCurrentLine(0);
	return true;
}

void AbstractDriver::setState(STATE state) {
	LOG(Logger::INFO, "setState(): %i:%s > %i:%s",state_,getStateString(state_).c_str(),state,getStateString(state).c_str());//TEMP was BULK
	state_ = state;
}

bool AbstractDriver::isPrinterOnline() const {
	STATE s = getState();
	return s != UNKNOWN && s != DISCONNECTED && s != CONNECTING;
}

int AbstractDriver::readData() {

	//LOG(Logger::BULK, "readData()");

	int rv = serial_.readData();
	log_.checkError(rv, "ABSD", "cannot read from device");
	if (rv == -2) {
		LOG(Logger::ERROR, "remote end closed connection, closing port");
		closeConnection();
		if (REQUEST_EXIT_ON_PORT_FAIL) server_.requestExit(1);
	} else if (rv == -1 && errno == ENXIO) {
		LOG(Logger::ERROR, "port was disconnected, closing port");
		closeConnection();
		if (REQUEST_EXIT_ON_PORT_FAIL) server_.requestExit(1);
	} else if (rv == -1 && errno == EBADF) {
		LOG(Logger::ERROR, "port file descriptor became invalid, closing port");
		closeConnection();
		if (REQUEST_EXIT_ON_PORT_FAIL) server_.requestExit(1);
	} else if (rv >= 0) {
		//LOG(Logger::BULK, "read %i bytes from device", rv);
	}
	return rv;
}

void AbstractDriver::setBaudrate(uint32_t baudrate) {
	baudrate_ = baudrate;
	Serial::ESERIAL_SET_SPEED_RESULT ssr = serial_.setSpeed(baudrate_);
	if(ssr == Serial::SSR_OK) {
		setState(CONNECTING);
	} else {
		LOG(Logger::ERROR,"  setting speed error");
	}
}

//TODO: add 57600?
void AbstractDriver::switchBaudrate() {
	setBaudrate((baudrate_ == B250000)? B115200 : B250000);
}

int AbstractDriver::findNumber(const string& code, size_t startPos) const {
	//LOG(Logger::BULK, "  findValue()");
	std::size_t posEnd = code.find('\n',startPos);
	//LOG(Logger::BULK, "    posEnd: %i", posEnd);
	//TODO: why is posEnd set to the nearest space if a newline could not be found?
	//      looks like it should look for a space first and only look for newline as backup instead
	if(posEnd == string::npos) {
		posEnd = code.find(' ',startPos);
		//LOG(Logger::BULK, "    posEnd>: %i", posEnd);
	}
	string valueStr = code.substr(startPos, posEnd-startPos);
	//LOG(Logger::BULK, "    valueStr: %s", valueStr.c_str());
	return ::atof(valueStr.c_str());
}

void AbstractDriver::extractGCodeInfo(const string& gcode) {
	//LOG(Logger::VERBOSE, "extractGCodeInfo()");
	//LOG(Logger::BULK, "  gcode: %s", gcode.c_str());

	// check for a heat command (M109 S... / M109 R...)
	std::size_t posHeat = gcode.rfind("M109");
	if(posHeat != std::string::npos) {
		targetTemperature_ = findNumber(gcode, posHeat + 6);
		LOG(Logger::VERBOSE, "  targetTemperature_: %i", targetTemperature_);
	}

	// check for a bed heat command (M190 S... / M190 R...)
	std::size_t posBedHeat = gcode.rfind("M190");
	if(posBedHeat != std::string::npos) {
		targetBedTemperature_ = findNumber(gcode, posBedHeat + 6);
		LOG(Logger::VERBOSE, "  targetBedTemperature_: %i", targetBedTemperature_);
	}
}
