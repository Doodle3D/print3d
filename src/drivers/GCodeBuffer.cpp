/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
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
#define LOG(lvl, fmt, ...) log_.log(lvl, "[MLD] " fmt, ##__VA_ARGS__)

#ifndef GCODE_BUFFER_MAX_SIZE_KB
# define GCODE_BUFFER_MAX_SIZE_KB 1024 * 3
#endif
#ifndef GCODE_BUFFER_SPLIT_SIZE_KB
# define GCODE_BUFFER_SPLIT_SIZE_KB 8
#endif

//private
const uint32_t GCodeBuffer::MAX_BUCKET_SIZE = 1024 * 50;
const uint32_t GCodeBuffer::MAX_BUFFER_SIZE = 1024 * GCODE_BUFFER_MAX_SIZE_KB;
const uint32_t GCodeBuffer::BUFFER_SPLIT_SIZE = 1024 * GCODE_BUFFER_SPLIT_SIZE_KB; //append will split its input on the first newline after this size

GCodeBuffer::GCodeBuffer()
: currentLine_(0), bufferedLines_(0), totalLines_(0), bufferSize_(0), log_(Logger::getInstance())
{
	LOG(Logger::VERBOSE, "init: max size: %luKiB, bucket size: %luKiB, split size: %luKiB",
		MAX_BUFFER_SIZE, MAX_BUCKET_SIZE, BUFFER_SPLIT_SIZE);
}

void GCodeBuffer::set(const string &gcode) {
	clear();
	append(gcode);
}

//NOTE: this function splits given code into chunk of approximately BUFFER_SPLIT_SIZE.
//without this, huge chunks (>1MB) would make repeated erase operations very inefficient.
void GCodeBuffer::append(const string &gcode) {
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
}

void GCodeBuffer::clear() {
	while (buckets_.size() > 0) {
		string *b = buckets_.front();
		delete b;
		buckets_.pop_front();
	}
	currentLine_ = bufferedLines_ = totalLines_ = 0;
	bufferSize_ = 0;
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
		if (i > 0 && !(i == 1 && posN == string::npos && *(b->rbegin()) != '\n')) return false;
	}

	line = b->substr(0, posN);
	return (posN != string::npos || line.length() > 0);
}

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
		if (i > 0 && !(i == 1 && pos == string::npos && *(b->rbegin()) != '\n')) return false;
	}

	size_t len = b->length();
	b->erase(0, (pos == string::npos) ? pos : pos + 1);
	bufferSize_ -= (len - b->length());

	if (b->length() == 0) {
		delete b;
		buckets_.pop_front();
	}

	bufferedLines_--;

	return pos != string::npos;
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
	std::size_t posComment = 0;
	while((posComment = buffer->find(';', pos)) != string::npos) {
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
	if (buffer->length() > 0) {
		char lastChar = buffer->at(buffer->length()-1);
		if(lastChar != '\n') {
			buffer->append("\n");
//			LOG(Logger::BULK, " add empty line at end");
		}
	}
//	LOG(Logger::BULK, "  ////////// >>>buffer: ");
//	LOG(Logger::BULK, "  \n%s\n////////// end >>>buffer",buffer->c_str());

	bufferSize_ -= (buflen - buffer->length());

	endTime = getMillis();
	LOG(Logger::BULK, "cleanupGCode(): took %lu ms (%lu removing comments, %lu removing double newlines)",
		endTime - startTime, commentDelta - startTime, doubleNLDelta - commentDelta);
}
