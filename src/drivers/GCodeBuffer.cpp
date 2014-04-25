/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include "GCodeBuffer.h"
#include <stdlib.h>
#include <string.h>
using std::string;

//NOTE: see Server.cpp for comments on this macro
#define LOG(lvl, fmt, ...) log_.log(lvl, "GCB ", fmt, ##__VA_ARGS__)

//private
const uint32_t GCodeBuffer::MAX_BUCKET_SIZE = 1024 * 50;
const uint32_t GCodeBuffer::MAX_BUFFER_SIZE = 1024 * 1024 * 3;

//Note: the GcodeSetResult texts are used all the way on the other end in javascript, consider this when changing them.
const string GCodeBuffer::GSR_NAMES[] = { "", "ok", "buffer_full", "seq_num_missing", "seq_num_mismatch", "seq_ttl_missing", "seq_ttl_mismatch", "seq_src_missing", "seq_src_mismatch" };
const size_t GCodeBuffer::GCODE_EXCERPT_LENGTH = 10;

GCodeBuffer::GCodeBuffer()
: currentLine_(0), bufferedLines_(0), totalLines_(0), bufferSize_(0),
  sequenceLastSeen_(-1), sequenceTotal_(-1), source_(0), log_(Logger::getInstance())
{
	LOG(Logger::BULK, "init - max_bucket_size: %lu, max_buffer_size: %lu", MAX_BUCKET_SIZE, MAX_BUFFER_SIZE);
}

/**
 * Sets given gcode by first calling clear(), then append(), returning its return value.
 * See append() for documentation.
 */
GCodeBuffer::GCODE_SET_RESULT GCodeBuffer::set(const string &gcode, const MetaData *metaData) {
	LOG(Logger::BULK, "set (i.e. clear+append)");
	clear();
	return append(gcode, metaData);
}

/**
 * Append given gcode to buffer, with optional consistency checks.
 * @param seqNum Optional sequence number; once given, it should increment by 1 in
 * 		each subsequent call until clear() is called.
 * @param metaData data for consistency checking, see below.
 *
 * @return GSR_OK if all went well, a relevant error status otherwise.
 *
 * Meta data may be omitted, any individual fields may also be omitted. If at
 * any point one of the fields is not -1/NULL, it should be passed in a consistent
 * way with each subsequent append call until the buffer is cleared again.
 * Consistent means for sequence numbers that they increment by exactly one and are
 * not greater than the total parameter (if given). The total must not change, and
 * the source must be the same text each time.
 *
 * NOTE: currently this function will never split given gcode into parts for
 * separate buckets, so MAX_BUCKET_SIZE is not a strict limit
 */
GCodeBuffer::GCODE_SET_RESULT GCodeBuffer::append(const string &gcode, const MetaData *metaData) {
	if (!metaData) LOG(Logger::BULK, "append - len: %zu, excerpt: %s", gcode.length(), gcode.substr(0, GCODE_EXCERPT_LENGTH).c_str());
	else LOG(Logger::BULK, "append - len: %zu, excerpt: %s, seq_num: %i, seq_ttl: %i, src: %s", gcode.length(), gcode.substr(0, GCODE_EXCERPT_LENGTH).c_str(),
			metaData->seqNumber, metaData->seqTotal, metaData->source ? metaData->source->c_str() : "(null)");

	if (sequenceLastSeen_ > -1) {
		if (!metaData || metaData->seqNumber < 0) return GSR_SEQ_NUM_MISSING;
		if (sequenceLastSeen_ + 1 != metaData->seqNumber) return GSR_SEQ_NUM_MISMATCH;
	}

	if (sequenceTotal_ > -1) {
		if (!metaData || metaData->seqTotal < 0) return GSR_SEQ_TTL_MISSING;
		if (sequenceTotal_ != metaData->seqTotal) return GSR_SEQ_TTL_MISMATCH;
		if (metaData->seqNumber + 1 > metaData->seqTotal) return GSR_SEQ_NUM_MISMATCH;
	}

	if (source_) {
		if (!metaData || !metaData->source) return GSR_SRC_MISSING;
		if (*source_ != *metaData->source) return GSR_SRC_MISMATCH;
	}

	if (getBufferSize() + gcode.length() > MAX_BUFFER_SIZE) {
		return GSR_BUFFER_FULL;
	}

	if (metaData) {
		sequenceLastSeen_ = metaData->seqNumber;
		sequenceTotal_ = metaData->seqTotal;
		if (!source_ && metaData->source) {
			source_ = new string(*metaData->source);
		}
	}


	/* finally add the gcode */

	if (buckets_.size() == 0) buckets_.push_back(new string());
	string *b = buckets_.back();
	if (b->length() >= MAX_BUCKET_SIZE) {
		b = new string();
		buckets_.push_back(b);
	}

	size_t pos = b->length();

	//make sure the gcode to be appended is never appended to an existing line
	if (pos > 0 && *(b->rbegin()) != '\n') {
		b->append("\n");
		pos++;
		bufferSize_++;
	}

	b->append(gcode);
	cleanupGCode(b, pos);
	bufferSize_ += gcode.length();
	updateStats(b, pos);

	return GSR_OK;
}

void GCodeBuffer::clear() {
	LOG(Logger::BULK, "clear");

	while (buckets_.size() > 0) {
		string *b = buckets_.front();
		delete b;
		buckets_.pop_front();
	}

	currentLine_ = bufferedLines_ = totalLines_ = 0;
	bufferSize_ = 0;

	sequenceLastSeen_ = -1;
	sequenceTotal_ = -1;
	if (source_) {
		delete source_;
		source_ = 0;
	}
}

int32_t GCodeBuffer::getCurrentLine() const {
	return currentLine_;
}

int32_t GCodeBuffer::getBufferedLines() const {
	return bufferedLines_;
}

int32_t GCodeBuffer::getTotalLines() const {
	return totalLines_;
}

//const string &GCodeBuffer::getBuffer() const {
//	return buffer_;
//}

int32_t GCodeBuffer::getBufferSize() const {
	return bufferSize_;
}

int32_t GCodeBuffer::getMaxBufferSize() const {
	return MAX_BUFFER_SIZE;
}

void GCodeBuffer::setCurrentLine(int32_t line) {
	currentLine_ = std::min(line, totalLines_);
}

//NOTE: FIXME: when requesting multiple lines, be aware that this function will currently
//return at most the first bucket, i.e. it does not concatenate data from multiple buffers.
bool GCodeBuffer::getNextLine(string &line, size_t amount) const {
	if (buckets_.size() == 0) {
		line = "";
		return false;
	}

	string *b = buckets_.front();
	size_t posN = b->find('\n');

	if (amount > 1) {
		size_t i;
		for (i = amount - 1; i > 0; i--) {
			posN = b->find('\n', posN + 1);
			if (posN == string::npos) break;
		}

		//if we breaked out of the loop, return false _unless_ we only needed one more line,
		//were at the end of the buffer and it was not terminated with a newline (i.e., we actually got this last line)
		if (i > 0 && !(i == 1 && posN == string::npos && *(b->rbegin()) != '\n')) return false;
	}

	line = b->substr(0, posN);
	return (posN != string::npos || line.length() > 0);
}

//FIXME: this function does currently not operate across bucket boundaries
bool GCodeBuffer::eraseLine(size_t amount) {
	if (buckets_.size() == 0) return false;

	string *b = buckets_.front();
	size_t pos = b->find('\n');

	if (amount > 1) {
		size_t i;
		for (i = amount - 1; i > 0; i--) {
			pos = b->find('\n', pos + 1);
			if (pos == string::npos) break;
		}
		if (i > 0 && !(i == 1 && pos == string::npos && *(b->rbegin()) != '\n')) return false; //see getNextLine for explanation
	}

	size_t len = b->length();
	b->erase(0, (pos == string::npos) ? pos : pos + 1);
	bufferSize_ -= (len - b->length());

	if (b->length() == 0) {
		delete b;
		buckets_.pop_front();
	}

	bufferedLines_ -= amount;

	return pos != string::npos;
}

//static
const std::string &GCodeBuffer::getGcodeSetResultString(GCODE_SET_RESULT gsr) {
	return GSR_NAMES[gsr];
}

/*********************
 * PRIVATE FUNCTIONS *
 *********************/

void GCodeBuffer::updateStats(string *buffer, size_t pos) {
	int32_t addedLineCount = std::count(buffer->begin() + pos, buffer->end(), '\n');
	if (buffer->length() > 0 && buffer->at(buffer->length() - 1) != '\n') addedLineCount++;
	bufferedLines_ += addedLineCount;
	totalLines_ += addedLineCount;
	if (currentLine_ > totalLines_) currentLine_ = totalLines_;
}

void GCodeBuffer::cleanupGCode(string *buffer, size_t pos) {
	//LOG(Logger::BULK, "  cleanupGCode");
	//LOG(Logger::BULK, "  buffer: ");
	//LOG(Logger::BULK, "  %s",buffer->c_str());

	size_t buflen = buffer->length();

	//replace \r with \n
	std::replace(buffer->begin() + pos, buffer->end(), '\r', '\n');

	//remove all comments (;...)
	std::size_t posComment = 0;
	while((posComment = buffer->find(';', pos)) != string::npos) {
		//LOG(Logger::BULK, "  posComment: %i",posComment);
		size_t posCommentEnd = buffer->find('\n', posComment);
		//LOG(Logger::BULK, "  posCommentEnd: %i",posCommentEnd);
		if(posCommentEnd == string::npos) {
			buffer->erase(posComment);
			//LOG(Logger::BULK, " erase until npos");
		} else {
			buffer->erase(posComment, posCommentEnd - posComment);
			//LOG(Logger::BULK, " erase: %i - %i",posComment,(posCommentEnd - posComment));
		}
	}

	//replace \n\n with \n
	std::size_t posDoubleNewline = pos;
	while((posDoubleNewline = buffer->find("\n\n", posDoubleNewline)) != string::npos) {
		buffer->replace(posDoubleNewline, 2, "\n");
	}

	// remove empty first line
	if(buffer->find("\n",0) == 0) {
		buffer->erase(0, 1);
	}

	//LOG(Logger::BULK, "  >buffer: ");
	//LOG(Logger::BULK, "  %s",buffer->c_str());

	bufferSize_ -= (buflen - buffer->length());
}
