/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef S3G_PARSER_H_SEEN
#define S3G_PARSER_H_SEEN

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "../server/Logger.h"

#ifndef LOG
# define LOG(lvl, fmt, ...) Logger::getInstance().log(lvl, "MBTD ", fmt, ##__VA_ARGS__)
#endif

using namespace std; //FIXME: do not do this in header

class S3GParser {
public:
	struct Command {
		string format,description;
	};

	int bufferPos, bufferSize, lineNumber;
	map<char,Command> commandTable;
	char *buffer;
	vector<string> commands;

	S3GParser()
	: bufferPos(0), bufferSize(0), lineNumber(0), buffer(0)
	{
		commandTable[129] = (Command) { "<iiiI",	"\t[129] Absolute move to (%i,%i,%i) at DDA %i" };
		commandTable[130] = (Command) { "<iii",		"\t[130] Machine position set as (%i,%i,%i)" };
		commandTable[131] = (Command) { "<BIH",		"\t[131] Home minimum on %X, feedrate %i, timeout %i s" };
		commandTable[132] = (Command) { "<BIH",		"\t[132] Home maximum on %X, feedrate %i, timeout %i s" };
		commandTable[133] = (Command) { "<I",		"\t[133] Delay of %i us" };
		commandTable[134] = (Command) { "<B",		"\t[134] Change extruder %i" };
		commandTable[135] = (Command) { "<BHH",		"\t[135] Wait until extruder %i ready (%i ms between polls, %i s timeout" };
		commandTable[136] = (Command) { "",			"printToolAction"};
		commandTable[137] = (Command) { "<B",		"\t[137] Enable/disable steppers %X" };
		commandTable[138] = (Command) { "<H",		"\t[138] User block on ID %i" };
		commandTable[139] = (Command) { "<iiiiiI",	"\t[139] Absolute move to (%i,%i,%i,%i,%i) at DDA %i" };
		commandTable[140] = (Command) { "<iiiii",	"\t[140] Set extended position as (%i,%i,%i,%i,%i)" };
		commandTable[141] = (Command) { "<BHH",		"\t[141] Wait for platform %i (%i ms between polls, %i s timeout)" };
		commandTable[142] = (Command) { "<iiiiiIB",	"\t[142] Move to (%i,%i,%i,%i,%i) in %i us (relative: %X)" };
		commandTable[143] = (Command) { "<b",		"\t[143] Store home position for axes %d" };
		commandTable[144] = (Command) { "<b",		"\t[144] Recall home position for axes %d" };
		commandTable[145] = (Command) { "<BB",		"\t[145] Set pot axis %i to %i" };
		commandTable[146] = (Command) { "<BBBBB",	"\t[146] Set RGB led red %i, green %i, blue %i, blink rate %i, effect %i" };
		commandTable[147] = (Command) { "<HHB",		"\t[147] Set beep, frequency %i, length %i, effect %i" };
		commandTable[148] = (Command) { "<BHB",		"\t[148] Pause for button 0x%X, timeout %i s, timeout_bevavior %i" };
		commandTable[149] = (Command) { "",			"\t[149] Display message, options 0x%X at %i,%i timeout %i\n '%s'" };
		commandTable[150] = (Command) { "<BB",		"\t[150] Set build percent %i%%, ignore %i" };
		commandTable[151] = (Command) { "<B",		"\t[151] Queue song %i" };
		commandTable[152] = (Command) { "<B",		"\t[152] Reset to factory, options 0x%X" };
		commandTable[153] = (Command) { "",			"\t[153] Start build, steps %i: %s" };
		commandTable[154] = (Command) { "<B",		"\t[154] End build, flags 0x%X" };
		commandTable[155] = (Command) { "<iiiiiIBfh", "\t[155] Move to (%i,%i,%i,%i,%i) dda_rate: %i (relative: %X) distance: %f feedrateX64: %i" };
		commandTable[156] = (Command) { "<B",		"\t[156] Set acceleration to %i" };
		commandTable[157] = (Command) { "<BBBIHHIIB", "\t[157] Stream version %i.%i (%i %i %i %i %i %i %i)" };
		commandTable[158] = (Command) { "<f",		"\t[158] Pause @ zPos %f" };
	}

	int calcsize(char c) { //http://docs.python.org/2/library/struct.html
		char chars[] = "@=<>!cbB?hHiIlLqQfdspP";
		char sizes[] = "0000011112244448848005";
		for (size_t i=0; i<sizeof(chars); i++) {
			if (chars[i]==c) return sizes[i]-'0';
		}
		//cout << c;
		cout << "warning calcsize(char): " << c << endl;
		return 0;
	}

	int calcsize(string format) {
		int size=0;
		for (size_t i=0; i<format.size(); i++) {
			size+=calcsize(format.at(i));
		}
		return size;
	}

	void parseToolAction() {
		string packetData;
		packetData.append(&buffer[bufferPos],3);

//		uint8_t index = buffer[bufferPos++];
//		uint8_t cmd = buffer[bufferPos++];
		bufferPos++; bufferPos++;

		uint8_t payloadLen = buffer[bufferPos++];

		packetData.append(&buffer[bufferPos],payloadLen);

		bufferPos+=payloadLen;
		commands.push_back(char(136) + packetData);
	}

	void parseDisplayMessageAction() {
		string msg;
		bufferPos+=4;
		while (buffer[bufferPos]!=0) {
			msg += buffer[bufferPos];
			bufferPos++;
		};
		commands.push_back(char(149) + msg);
	}

	void parseBuildStartNotificationAction() {
		string buildName;
		bufferPos+=4;
		while (buffer[bufferPos]!=0) {
			buildName += buffer[bufferPos];
			bufferPos++;
		};
		commands.push_back(char(153) + buildName);
	}

	bool parseNextCommand() {
		if (bufferPos>=bufferSize) {
			//LOG(Logger::VERBOSE, "parseNextCommand(): nothing to do; bufferPos: %i/%i", bufferPos, bufferSize);
			return false;
		}
		uint8_t command = buffer[bufferPos++];
		//LOG(Logger::VERBOSE, "parseNextCommand(): %3i; buffer: %i/%i", command, bufferPos, bufferSize);

		if (command==136) parseToolAction();
		else if (command==149) parseDisplayMessageAction();
		else if (command==153) parseBuildStartNotificationAction();
		else {
			const string &packetFormat = commandTable[command].format;
			//const string &packetDescription = commandTable[command].description;
			int packetLen = calcsize(packetFormat);
			string packetData;
			packetData.append(&buffer[bufferPos],packetLen);
			bufferPos+=packetLen;
			commands.push_back(char(command) + packetData);
		}

		lineNumber++;
		return true;
	}

	void setBuffer(char *buf, int buflen) {
		buffer = buf;
		bufferSize = buflen;
		bufferPos = 0;
		lineNumber = 0;
	}

	int getLineNumber() {
		return lineNumber;
	}

//	void loadFile(string filename) {
//		if (!ofFile(filename).exists()) {
//			cout << filename << " not found" << endl;
//			std::exit(1);
//		}
//		commands.clear();
//		lineNumber = 0;
//		ofBuffer fileBuffer = ofBufferFromFile(filename,true);
//		buffer = fileBuffer.getBinaryBuffer();
//		bufferSize = fileBuffer.size();
//		bufferPos = 0;
//		if (buffer==NULL) {
//			cout << "loadFile(" << filename << "): buffer is empty" << endl;
//		} else {
//			while (parseNextCommand());
//		}
//	}
};

#undef LOG

#endif /* ! S3G_PARSER_H_SEEN */
