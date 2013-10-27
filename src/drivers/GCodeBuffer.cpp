#include "GCodeBuffer.h"
#include <stdlib.h>
#include <string.h>
using std::string;

//NOTE: see Server.cpp for comments on this macro
#define LOG(lvl, fmt, ...) log_.log(lvl, "[MLD] " fmt, ##__VA_ARGS__)

//private
const uint32_t GCodeBuffer::MAX_BUCKET_SIZE = 1024 * 50;
const uint32_t GCodeBuffer::MAX_BUFFER_SIZE = 1024 * 1024 * 3;

GCodeBuffer::GCodeBuffer()
: currentLine_(0), bufferedLines_(0), totalLines_(0), bufferSize_(0), log_(Logger::getInstance())
{ /* empty */ }

void GCodeBuffer::set(const string &gcode) {
	clear();
	append(gcode);
}

//NOTE: currently this function will never split given gcode into parts for
//separate buckets, so MAX_BUCKET_SIZE is not a strict limit
void GCodeBuffer::append(const string &gcode) {

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
