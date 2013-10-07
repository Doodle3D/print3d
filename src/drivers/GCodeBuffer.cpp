#include "GCodeBuffer.h"

using std::string;


GCodeBuffer::GCodeBuffer()
: currentLine_(0), numLines_(0)
{ /* empty */ }

void GCodeBuffer::set(const string &gcode) {
	buffer_ = gcode;
	cleanupGCode(0);
	numLines_ = 0;
	updateStats(0);
}

void GCodeBuffer::append(const string &gcode) {
	size_t pos = buffer_.length();
	buffer_ += gcode;
	cleanupGCode(pos);
	updateStats(pos);
}

void GCodeBuffer::clear() {
	buffer_.clear();
	numLines_ = 0;
	updateStats(0);
}

int32_t GCodeBuffer::getCurrentLine() const {
	return currentLine_;
}

int32_t GCodeBuffer::getNumLines() const {
	return numLines_;
}

const string &GCodeBuffer::getBuffer() const {
	return buffer_;
}

int32_t GCodeBuffer::getBufferSize() const {
	return buffer_.length();
}

void GCodeBuffer::setCurrentLine(int32_t line) {
	currentLine_ = std::min(line, numLines_);
}

bool GCodeBuffer::getNextLine(string &line) const {
	size_t posN = buffer_.find('\n');
	line = buffer_.substr(0, posN);
	return (posN != string::npos || line.length() > 0);
}

bool GCodeBuffer::eraseLine() {
	size_t pos = buffer_.find('\n');

	buffer_.erase(0, (pos == string::npos) ? pos : pos + 1);

	numLines_--;
	if (currentLine_ > numLines_) currentLine_ = numLines_;

	return pos != string::npos;
}

/*********************
 * PRIVATE FUNCTIONS *
 *********************/

void GCodeBuffer::updateStats(size_t pos) {
	numLines_ += std::count(buffer_.begin() + pos, buffer_.end(), '\n');
	if (buffer_.length() > 0 && buffer_.at(buffer_.length() - 1) != '\n') numLines_++;
	if (currentLine_ > numLines_) currentLine_ = numLines_;
}

void GCodeBuffer::cleanupGCode(size_t pos) {
	//replace \r with \n
	std::replace(buffer_.begin(), buffer_.end(), '\r', '\n');

	//replace \n\n with \n
	std::size_t posDoubleNewline = 0;
	while((posDoubleNewline = buffer_.find("\n\n", posDoubleNewline)) != string::npos) {
		buffer_.replace(posDoubleNewline, 2, "\n");
	}

	//remove all comments (;...)
	std::size_t posComment = 0;
	while((posComment = buffer_.find(';')) != string::npos) {
		size_t posCommentEnd = buffer_.find('\n', posComment);

		if(posCommentEnd == string::npos) buffer_.erase(posComment);
		else buffer_.erase(posComment, posCommentEnd - posComment);
	}
}
