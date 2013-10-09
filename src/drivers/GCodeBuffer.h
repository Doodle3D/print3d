#ifndef GCODE_BUFFER_H_SEEN
#define GCODE_BUFFER_H_SEEN

#include <stdint.h>
#include <string>
#include <deque>

class GCodeBuffer {
public:
	static const uint32_t MAX_BUFFER_SIZE;

	typedef std::deque<std::string*> deque_stringP;

	GCodeBuffer();

  void set(const std::string &gcode);
  void append(const std::string &gcode);
  void clear();

	int32_t getCurrentLine() const;
	int32_t getBufferedLines() const;
	int32_t getTotalLines() const;
//	const std::string &getBuffer() const;
//	int32_t getBufferSize() const;

	void setCurrentLine(int32_t line);

	bool getNextLine(std::string &line) const;
	bool eraseLine();

private:
	deque_stringP buffers_;
	int32_t currentLine_;
	int32_t bufferedLines_;
	int32_t totalLines_;

	void updateStats(std::string *buffer, size_t pos);
	void cleanupGCode(std::string *buffer, size_t pos);
};

#endif /* ! GCODE_BUFFER_H_SEEN */
