/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef GCODE_BUFFER_H_SEEN
#define GCODE_BUFFER_H_SEEN

#include <stdint.h>
#include <string>
#include <deque>
#include "../server/Logger.h"

class GCodeBuffer {
public:
	typedef std::deque<std::string*> deque_stringP;

	typedef enum GCODE_SET_RESULT {
		/* value 0 is intentionally left out */
		GSR_OK = 1,
		GSR_BUFFER_FULL,
		GSR_SEQ_NUM_MISSING,
		GSR_SEQ_NUM_MISMATCH,
		GSR_SEQ_TTL_MISSING,
		GSR_SEQ_TTL_MISMATCH,
		GSR_SRC_MISSING,
		GSR_SRC_MISMATCH
	} GCODE_SET_RESULT;

	struct MetaData {
		MetaData() : seqNumber(-1), seqTotal(-1), source(0) {}

		int32_t seqNumber;
		int32_t seqTotal;
		std::string *source;
	};


	GCodeBuffer();

	void setKeepGpxMacroComments(bool keep);

	GCODE_SET_RESULT set(const std::string &gcode, int32_t totalLines = -1, const MetaData *metaData = 0);
	GCODE_SET_RESULT append(const std::string &gcode, int32_t totalLines = -1, const MetaData *metaData = 0);
	void clear();

	int32_t getCurrentLine() const;
	int32_t getBufferedLines() const;
	int32_t getTotalLinesSent() const;
	int32_t getTotalLines() const;
//	const std::string &getBuffer() const;
	int32_t getBufferSize() const;
	int32_t getMaxBufferSize() const;

	const MetaData *getMetaData() const;

	void setCurrentLine(int32_t line);

	int32_t getNextLine(std::string &line, size_t amount = 1) const;
	int32_t eraseLine(size_t amount = 1);

	static const std::string &getGcodeSetResultString(GCODE_SET_RESULT gsr);

private:
	static const uint32_t MAX_BUCKET_SIZE;
	static const uint32_t MAX_BUFFER_SIZE;
	static const uint32_t BUFFER_SPLIT_SIZE;

	static const std::string GSR_NAMES[];
	static const size_t GCODE_EXCERPT_LENGTH;

	deque_stringP buckets_;
	int32_t currentLine_;
	int32_t bufferedLines_;
	int32_t totalLinesSent_;
	int32_t explicitTotalLines_;
	int32_t bufferSize_;

	MetaData md_;

	bool keepGpxMacroComments_;

	Logger& log_;

	void appendChunk(const std::string &gcode);
	void updateStats(std::string *buffer, size_t pos);
	void cleanupGCode(std::string *buffer, const size_t pos);
};

#endif /* ! GCODE_BUFFER_H_SEEN */
