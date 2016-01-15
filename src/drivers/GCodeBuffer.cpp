/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include "GCodeBuffer.h"
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "../utils.h"
using std::string;

//NOTE: see Server.cpp for comments on this macro
#define LOG(lvl, fmt, ...) log_.log(lvl, "GCB ", fmt, ##__VA_ARGS__)

#ifndef GCODE_BUFFER_MAX_SIZE_KB
# define GCODE_BUFFER_MAX_SIZE_KB 1024 * 3
#endif
#ifndef GCODE_BUFFER_SPLIT_SIZE_KB
# define GCODE_BUFFER_SPLIT_SIZE_KB 8
#endif

//private
const uint32_t GCodeBuffer::MAX_BUCKET_SIZE = 1024 * 50;
const uint32_t GCodeBuffer::MAX_BUFFER_SIZE = 1024 * GCODE_BUFFER_MAX_SIZE_KB; //set to 0 to disable (TODO: actually, this isn't used at all currently)
const uint32_t GCodeBuffer::BUFFER_SPLIT_SIZE = 1024 * GCODE_BUFFER_SPLIT_SIZE_KB; //append will split its input on the first newline after this size

//Note: the GcodeSetResult texts are used all the way on the other end in javascript, consider this when changing them.
const string GCodeBuffer::GSR_NAMES[] = { "", "ok", "buffer_full", "seq_num_missing", "seq_num_mismatch", "seq_ttl_missing", "seq_ttl_mismatch", "seq_src_missing", "seq_src_mismatch" };
const size_t GCodeBuffer::GCODE_EXCERPT_LENGTH = 10;

GCodeBuffer::GCodeBuffer()
: currentLine_(0), bufferedLines_(0), totalLines_(0), bufferSize_(0),
  sequenceLastSeen_(-1), sequenceTotal_(-1), source_(0),
  keepGpxMacroComments_(false), log_(Logger::getInstance())
{
	LOG(Logger::VERBOSE, "init - max size: %luKiB, bucket size: %luKiB, split size: %luKiB",
		MAX_BUFFER_SIZE, MAX_BUCKET_SIZE, BUFFER_SPLIT_SIZE);
}

/**
 * When passed true, gcode cleanup will not touch GPX macro comments (';@...').
 */
void GCodeBuffer::setKeepGpxMacroComments(bool keep) {
	keepGpxMacroComments_ = keep;
}

/**
 * Sets given gcode by first calling clear(), then append(), returning its return value.
 * See append() for documentation.
 */
GCodeBuffer::GCODE_SET_RESULT GCodeBuffer::set(const string &gcode, const MetaData *metaData) {
	//LOG(Logger::VERBOSE, "set (i.e. clear+append)");
	clear();
	return append(gcode, metaData);
}

/**
 * Append given gcode to buffer, with optional consistency checks.
 *
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
 * NOTE: this function splits given code into chunk of approximately BUFFER_SPLIT_SIZE.
 * without this, huge chunks (>1MB) would make repeated erase operations very inefficient.
 */
GCodeBuffer::GCODE_SET_RESULT GCodeBuffer::append(const string &gcode, const MetaData *metaData) {
	if (!metaData) {
		LOG(Logger::VERBOSE, "append - len: %zu, excerpt: '%s'; ttl size(lines): %d (%d)",
				gcode.length(), gcode.substr(0, GCODE_EXCERPT_LENGTH).c_str(), bufferSize_, bufferedLines_);
	} else {
		LOG(Logger::VERBOSE, "append - len: %zu, excerpt: %s, seq_num: %i, seq_ttl: %i, src: %s; ttl size (lines): %d (%d)",
				gcode.length(), gcode.substr(0, GCODE_EXCERPT_LENGTH).c_str(),
				metaData->seqNumber, metaData->seqTotal, metaData->source ? metaData->source->c_str() : "(null)",
				bufferSize_, bufferedLines_);
	}

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

	uint32_t startTime = getMillis();
	int count = 0;
	size_t size = gcode.size();
	size_t start = 0;

	while(start < gcode.size()) {
		size_t len = BUFFER_SPLIT_SIZE;
		size_t nl = gcode.find('\n', start + len);

		len = (nl != string::npos) ? nl - start + 1 : gcode.size() - start;
		appendChunk(gcode.substr(start, len));
		count++;
		start += len;
	}

	LOG(Logger::VERBOSE, "append(): added %zu bytes of gcode (%i chunks) in %lu ms",
		size, count, getMillis() - startTime);

	return GSR_OK;
}

void GCodeBuffer::clear() {
	LOG(Logger::VERBOSE, "clear");

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
//NOTE: if the requested amount of lines is not present, as many as possible will be returned.
int32_t GCodeBuffer::getNextLine(string &line, size_t amount) const {
	if (buckets_.size() == 0) {
		line = "";
		return 0;
	}

	string *b = buckets_.front();
	size_t pos = b->find('\n');
	int32_t counter = pos != string::npos ? 1 : 0;

	if (amount > 1 && pos != string::npos) {
		size_t i;
		for (i = amount - 1; i > 0; i--) {
			pos = b->find('\n', pos + 1);
			if (pos == string::npos) break;
			counter++;
		}

		if (pos == string::npos && *(b->rbegin()) != '\n') counter++; //account for unterminated line at end of buffer
	}

	line = b->substr(0, pos);

	return counter;
}

//FIXME: this function does currently not operate across bucket boundaries
//NOTE: if amount of lines is not present, remove as many as possible
int32_t GCodeBuffer::eraseLine(size_t amount) {
	if (buckets_.size() == 0) return 0;

	string *b = buckets_.front();
	size_t pos = b->find('\n');
	int32_t counter = 1;

	if (amount > 1) {
		size_t i;
		for (i = amount - 1; i > 0; i--) {
			pos = b->find('\n', pos + 1);
			if (pos == string::npos) break;
			counter++;
		}

		if (pos == string::npos && *(b->rbegin()) != '\n') counter++; //account for unterminated line at end of buffer
	}

	size_t len = b->length();
	b->erase(0, (pos == string::npos) ? pos : pos + 1);
	bufferSize_ -= (len - b->length());

	if (b->length() == 0) {
		delete b;
		buckets_.pop_front();
	}

	bufferedLines_ -= counter;

	return counter;
}

//static
const std::string &GCodeBuffer::getGcodeSetResultString(GCODE_SET_RESULT gsr) {
	return GSR_NAMES[gsr];
}


/*********************
 * PRIVATE FUNCTIONS *
 *********************/

void GCodeBuffer::appendChunk(const string &gcode) {
	if (buckets_.size() == 0) buckets_.push_back(new string());
	string *b = buckets_.back();
	if (b->length() >= MAX_BUCKET_SIZE) {
		b = new string();
		buckets_.push_back(b);
	}

	size_t pos = b->length();
	b->append(gcode);
	cleanupGCode(b, pos);
	bufferSize_ += gcode.length();
	updateStats(b, pos);
}

void GCodeBuffer::updateStats(string *buffer, size_t pos) {
	int32_t addedLineCount = std::count(buffer->begin() + pos, buffer->end(), '\n');
	if (buffer->length() > 0 && buffer->at(buffer->length() - 1) != '\n') addedLineCount++;
	bufferedLines_ += addedLineCount;
	totalLines_ += addedLineCount;
	if (currentLine_ > totalLines_) currentLine_ = totalLines_;
}

void GCodeBuffer::cleanupGCode(string *buffer, size_t pos) {
	uint32_t startTime = getMillis(), commentDelta, doubleNLDelta, endTime;

//	LOG(Logger::BULK, "cleanupGCode");
//	LOG(Logger::BULK, "  pos: %i",pos);
//	LOG(Logger::BULK, "  ////////// buffer: ");
//	LOG(Logger::BULK, "  \n%s\n////////// end buffer",buffer->c_str());
	size_t buflen = buffer->length();

	//replace \r with \n
	std::replace(buffer->begin() + pos, buffer->end(), '\r', '\n');

	//remove all comments (;...)
	std::size_t posComment = pos;
	while((posComment = buffer->find(';', posComment)) != string::npos) {
		if (keepGpxMacroComments_ && posComment < buffer->length() - 1 && buffer->at(posComment + 1) == '@') {
			LOG(Logger::INFO, "found macro comment, skipping from %i to %i", posComment, posComment + 1);
			posComment++;
			continue;
		}

//		LOG(Logger::BULK, "  posComment: %i",posComment);
		size_t posCommentEnd = buffer->find('\n', posComment);
//		LOG(Logger::BULK, "  posCommentEnd: %i",posCommentEnd);
		if(posCommentEnd == string::npos) {
			buffer->erase(posComment);
//			LOG(Logger::BULK, " erase until npos");
		} else {
			buffer->erase(posComment, posCommentEnd - posComment);
//			LOG(Logger::BULK, " erase: %i - %i",posComment,(posCommentEnd - posComment));
		}

		if (buffer->empty()) return;

		posComment = pos;
	}

	commentDelta = getMillis();

	//replace \n\n with \n
	std::size_t posDoubleNewline = pos;
	// -1 to make sure we also check the first line of the part we're checking
	if(posDoubleNewline > 0) posDoubleNewline--;
	while((posDoubleNewline = buffer->find("\n\n", posDoubleNewline)) != string::npos) {
//		LOG(Logger::BULK, " erase double lines: %i",posDoubleNewline);
		buffer->replace(posDoubleNewline, 2, "\n");
	}

	doubleNLDelta = getMillis();

	// remove empty first line
	if(buffer->find("\n",0) == 0) {
		buffer->erase(0, 1);
		LOG(Logger::BULK, " erase first empty line");
	}

	// Make sure we end with an empty line except when the buffer is empty
	if(buffer->length() > 0 && *(buffer->rbegin()) != '\n') {
		buffer->append("\n");
//		LOG(Logger::BULK, " add empty line at end");
	}
//	LOG(Logger::BULK, "  ////////// >>>buffer: ");
//	LOG(Logger::BULK, "  \n%s\n////////// end >>>buffer",buffer->c_str());

	bufferSize_ -= (buflen - buffer->length());

	endTime = getMillis();
	LOG(Logger::BULK, "cleanupGCode(): took %lu ms (%lu removing comments, %lu removing double newlines)",
		endTime - startTime, commentDelta - startTime, doubleNLDelta - commentDelta);
}
