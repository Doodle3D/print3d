/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef MAKERBOT_DRIVER_H_SEEN
#define MAKERBOT_DRIVER_H_SEEN

#include <deque>
#include <string>
#include <vector>
#include "AbstractDriver.h"
#include "S3GParser.h"
#include "../Timer.h"
#include "../server/Logger.h"

class MakerbotDriver : public AbstractDriver {
public:
	MakerbotDriver(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

	static const AbstractDriver::DriverInfo& getDriverInfo();
	virtual int update();

	size_t convertGCode(const std::string &gcode); //TEMP

	//overrides
	GCodeBuffer::GCODE_SET_RESULT setGCode(const std::string& gcode, GCodeBuffer::MetaData *metaData = 0);
	void clearGCode();

	//overrides
	int32_t getCurrentLine() const;
	int32_t getBufferedLines() const;

	static AbstractDriver* create(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

protected:
	bool startPrint(const std::string& gcode, STATE state = PRINTING);
	bool stopPrint(const std::string& endcode);
	bool startPrint(STATE state = PRINTING);
	bool stopPrint();
	bool resetPrint();
	void sendCode(const std::string& code);
	void readResponseCode(std::string& code);
	void fullStop();

private:
	static const int MAX_GPX_BUFFER_SIZE;
	static const int PRINTER_BUFFER_SIZE;
	static const size_t QUEUE_MIN_SIZE;
	static const size_t QUEUE_FILL_SIZE;;
	static const int GCODE_CVT_LINES;

	Timer timer_;
	S3GParser parser_;

	uint32_t bufferSpace_;
	std::deque<std::string> queue_;

	float cmdToLineRatio_;
	bool validResponseReceived_;

	unsigned int firmwareVersion_;

	void processQueue();
	uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data);
	std::string getResponseMessage(int code);
	void queueCommands(std::vector<std::string> commands);
	bool updateTemperatures();
	int getFirmwareVersion();
	int requestBufferSpace();
	void playSong(uint8_t song=0);
	void resetPrinterBuffer();
	void abort();

	int parseResponse(int cmd, int toolcmd = -1);
	bool sendPacket(uint8_t *payload, int len, bool updateBufferSpace = true);
	int readAndCheckError(int timeout);
	int readAndCheckError(unsigned char *buf, size_t buflen, int timeout);
	void handleReadError(int rv);
	uint16_t read16(unsigned char *buf);
	uint32_t read32(unsigned char *buf);
};

#endif /* ! MAKERBOT_DRIVER_H_SEEN */
