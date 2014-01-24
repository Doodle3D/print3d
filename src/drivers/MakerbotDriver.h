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

	void clearGpxBuffer();
	size_t convertGcode(const std::string &gcode);//TEMP
	//overrides
	void setGCode(const std::string& gcode);
	void appendGCode(const std::string& gcode);
	void clearGCode();

	int32_t getCurrentLine() const;
	int32_t getBufferedLines() const;
	int32_t getTotalLines() const;

	static AbstractDriver* create(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

protected:
	void startPrint(const std::string& gcode, STATE state = PRINTING);
	void stopPrint(const std::string& endcode);
	void startPrint(STATE state = PRINTING);
	void stopPrint();
	void resetPrint();
	void sendCode(const std::string& code);
	void readResponseCode(std::string& code);

private:
//	static const int WAIT_GCODE_LINES;
	static const int MAX_GPX_BUFFER_SIZE;

	Timer timer_;
	S3GParser parser_;

	unsigned char *gpxBuffer_;
	size_t gpxBufferSize_;

	uint32_t bufferSpace_;
	std::deque<std::string> queue_;

	size_t currentCmd_, totalCmds_;
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
