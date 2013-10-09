#include "GCodeBuffer.h"

using std::string;

const uint32_t GCodeBuffer::MAX_BUFFER_SIZE = 1024 * 50;

GCodeBuffer::GCodeBuffer()
: currentLine_(0), bufferedLines_(0), totalLines_(0)
{ /* empty */ }

void GCodeBuffer::set(const string &gcode) {
	clear();
	append(gcode);
}

void GCodeBuffer::append(const string &gcode) {

	if (buffers_.size() == 0) buffers_.push_back(new string());
	string *b = buffers_.back();
	if (b->length() >= MAX_BUFFER_SIZE) {
		b = new string();
		buffers_.push_back(b);
	}

	size_t pos = b->length();
	b->append(gcode);
	cleanupGCode(b, pos);
	updateStats(b, pos);
}

void GCodeBuffer::clear() {
	while (buffers_.size() > 0) {
		string *b = buffers_.front();
		delete b;
		buffers_.pop_front();
	}
	currentLine_ = bufferedLines_ = totalLines_ = 0;
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
//
//int32_t GCodeBuffer::getBufferSize() const {
//	return buffer_.length();
//}

void GCodeBuffer::setCurrentLine(int32_t line) {
	currentLine_ = std::min(line, totalLines_);
}

bool GCodeBuffer::getNextLine(string &line) const {
	if (buffers_.size() == 0) {
		line = "";
		return false;
	}

	string *b = buffers_.front();
	size_t posN = b->find('\n');
	line = b->substr(0, posN);
	return (posN != string::npos || line.length() > 0);
}

bool GCodeBuffer::eraseLine() {
	if (buffers_.size() == 0) return false;

	string *b = buffers_.front();
	size_t pos = b->find('\n');

	b->erase(0, (pos == string::npos) ? pos : pos + 1);

	if (b->length() == 0) {
		delete b;
		buffers_.pop_front();
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
	//replace \r with \n
	std::replace(buffer->begin() + pos, buffer->end(), '\r', '\n');

	//remove all comments (;...)
	std::size_t posComment = 0;
	while((posComment = buffer->find(';', pos)) != string::npos) {
		size_t posCommentEnd = buffer->find('\n', posComment);

		if(posCommentEnd == string::npos) buffer->erase(posComment);
		else buffer->erase(posComment, posCommentEnd - posComment);
	}

	//replace \n\n with \n
	std::size_t posDoubleNewline = pos;
	while((posDoubleNewline = buffer->find("\n\n", posDoubleNewline)) != string::npos) {
		buffer->replace(posDoubleNewline, 2, "\n");
	}
}
