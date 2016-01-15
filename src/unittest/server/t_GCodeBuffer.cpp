#include <string>
#include <fructose/fructose.h>
#include "../../drivers/GCodeBuffer.h"

using std::string;

struct t_GCodeBuffer : public fructose::test_base<t_GCodeBuffer> {
	void testSet(const string& test_name) {
		GCodeBuffer buffer;

		fructose_assert_eq(buffer.getTotalLines(), 0);
		fructose_assert_no_exception(buffer.set("")); //actually, none of the buffer's functions should throw exceptions
		fructose_assert_eq(buffer.getTotalLines(), 0);

		fructose_assert_eq(buffer.getTotalLines(), 0);
		buffer.set("abc");
		fructose_assert_eq(buffer.getTotalLines(), 1);

		buffer.clear();
		fructose_assert_eq(buffer.getTotalLines(), 0);
		buffer.set("abc\n");
		fructose_assert_eq(buffer.getTotalLines(), 1);

		buffer.clear();
		fructose_assert_eq(buffer.getTotalLines(), 0);
		buffer.set("abc\n");
		fructose_assert_eq(buffer.getTotalLines(), 1);
	}

	void testAppend(const string& test_name) {
		GCodeBuffer buffer;

		buffer.set("abc\n");
		buffer.append("xyz\nnext line");
		fructose_assert_eq(buffer.getTotalLines(), 3);

		buffer.clear();
		buffer.append("def\nmore data\n");
		fructose_assert_eq(buffer.getTotalLines(), 2);
	}

	void testAppendWithoutNewlines(const string& test_name) {
		GCodeBuffer buffer;
		string lineBuf;

		buffer.set("abc");
		buffer.append("def");
		fructose_assert_eq(buffer.getTotalLines(), 2);

		buffer.getNextLine(lineBuf); buffer.eraseLine();
		fructose_assert_eq(lineBuf, "abc");
		buffer.getNextLine(lineBuf); buffer.eraseLine();
		fructose_assert_eq(lineBuf, "def");
	}

	//Note: what is not being tested here, is appending gcode without sequence numbers
	//first and after that append *with* them. This is allowed as long as the numbers are valid.
	void testAppendConsistencyChecks(const string& test_name) {
		GCodeBuffer buffer;
		GCodeBuffer::MetaData md;

		md.seqNumber = 0; md.seqTotal = 3; fructose_assert_eq(buffer.set("abc", &md), GCodeBuffer::GSR_OK);

		//check proper functioning of using sequence numbers
		fructose_assert_eq(buffer.append("def_no1", &md), GCodeBuffer::GSR_SEQ_NUM_MISMATCH);
		md.seqNumber = -1;
		fructose_assert_eq(buffer.append("def_no2", &md), GCodeBuffer::GSR_SEQ_NUM_MISSING);
		md.seqNumber = 1; md.seqTotal = 4;
		fructose_assert_eq(buffer.append("def_no3", &md), GCodeBuffer::GSR_SEQ_TTL_MISMATCH);
		md.seqTotal = -1;
		fructose_assert_eq(buffer.append("def_no4", &md), GCodeBuffer::GSR_SEQ_TTL_MISSING);
		md.seqNumber = 2; md.seqTotal = 3;
		fructose_assert_eq(buffer.append("def_no5", &md), GCodeBuffer::GSR_SEQ_NUM_MISMATCH);
		md.seqNumber = 1;
		fructose_assert_eq(buffer.append("def", &md), GCodeBuffer::GSR_OK);

		fructose_assert_eq(buffer.append("ghi_no1", &md), GCodeBuffer::GSR_SEQ_NUM_MISMATCH);
		md.seqNumber = 2;
		fructose_assert_eq(buffer.append("ghi", &md), GCodeBuffer::GSR_OK);
		md.seqNumber = 3;
		fructose_assert_eq(buffer.append("ghi_no2", &md), GCodeBuffer::GSR_SEQ_NUM_MISMATCH);

		//check if the data has been appended properly
		string lineBuf;
		fructose_assert_eq(buffer.getTotalLines(), 3);
		fructose_assert_eq(buffer.getNextLine(lineBuf), 1);
		fructose_assert_eq(lineBuf, "abc");
		buffer.eraseLine();
		fructose_assert_eq(buffer.getNextLine(lineBuf), 1);
		fructose_assert_eq(lineBuf, "def");
		buffer.eraseLine();
		fructose_assert_eq(buffer.getNextLine(lineBuf), 1);
		fructose_assert_eq(lineBuf, "ghi");
		buffer.eraseLine();

		//check passing along a source name
		string srcName1 = "from_here", srcName2 = "from_there";
		buffer.clear();
		md.seqNumber = 0; md.seqTotal = 3;
		fructose_assert_eq(buffer.set("abc", &md), GCodeBuffer::GSR_OK);
		md.seqNumber = 1; md.source = &srcName1;
		fructose_assert_eq(buffer.append("def", &md), GCodeBuffer::GSR_OK);
		md.seqNumber = 2; md.source = 0;
		fructose_assert_eq(buffer.append("ghi_no1", &md), GCodeBuffer::GSR_SRC_MISSING);
		md.source = &srcName2;
		fructose_assert_eq(buffer.append("ghi_no2", &md), GCodeBuffer::GSR_SRC_MISMATCH);
		md.source = &srcName1;
		fructose_assert_eq(buffer.append("ghi", &md), GCodeBuffer::GSR_OK);

		//check if everything resets properly
		buffer.clear();
		fructose_assert_eq(buffer.set("abc"), GCodeBuffer::GSR_OK);
	}

	void testGetErase(const string& test_name) {
		GCodeBuffer buffer;

		const string line1 = "line 1", line2 = "line 2", line3 = "line 3", line4 = "line 4";
		buffer.append(line1 + "\n" + line2 + "\n" + line3 + "\n" + line4);
		fructose_assert_eq(buffer.getTotalLines(), 4);

		string lineBuf;
		int32_t rv;

		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, 1);
		fructose_assert_eq(lineBuf, line1);
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, 1);
		fructose_assert_eq(lineBuf, line1);

		buffer.eraseLine();
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, 1);
		fructose_assert_eq(lineBuf, line2);

		buffer.eraseLine();
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, 1);
		fructose_assert_eq(lineBuf, line3);

		buffer.eraseLine();
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, 1);
		fructose_assert_eq(lineBuf, line4);

		buffer.eraseLine();
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, 0);
		fructose_assert_eq(lineBuf, "");
	}

	void testCleanup(const string& test_name) {
		GCodeBuffer buffer;
		string lineBuf;
		int32_t rv;

		buffer.set("line   ;  with comment");
		fructose_assert_eq(buffer.getTotalLines(), 1);
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, 1);
		fructose_assert_eq(lineBuf, "line   ");

		buffer.set(";  pure comment");
		fructose_assert_eq(buffer.getTotalLines(), 0);
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(lineBuf, "");
		fructose_assert_eq(rv, 0);

		buffer.set("car+new mix\r\n\r\n\n\n\n");
		fructose_assert_eq(buffer.getTotalLines(), 1);
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, 1);
		fructose_assert_eq(lineBuf, "car+new mix");
	}

	void testLineCounting(const string& test_name) {
		GCodeBuffer buffer;

		fructose_assert_eq(buffer.getCurrentLine(), 0);
		fructose_assert_eq(buffer.getBufferedLines(), 0);
		fructose_assert_eq(buffer.getTotalLines(), 0);
		buffer.setCurrentLine(42);
		fructose_assert_eq(buffer.getCurrentLine(), 0);

		buffer.set("line 1\nline 2\n line 3");
		fructose_assert_eq(buffer.getCurrentLine(), 0);
		fructose_assert_eq(buffer.getBufferedLines(), 3);
		fructose_assert_eq(buffer.getTotalLines(), 3);
		buffer.setCurrentLine(3);
		fructose_assert_eq(buffer.getCurrentLine(), 3);

		buffer.eraseLine();
		fructose_assert_eq(buffer.getCurrentLine(), 3);
		fructose_assert_eq(buffer.getBufferedLines(), 2);
		fructose_assert_eq(buffer.getTotalLines(), 3);

		buffer.clear();
		fructose_assert_eq(buffer.getCurrentLine(), 0);
		fructose_assert_eq(buffer.getBufferedLines(), 0);
		fructose_assert_eq(buffer.getTotalLines(), 0);
	}

	void testBufferSize(const string& test_name) {
		GCodeBuffer buffer;
		const string text1 = "line1\nline2\nline3\n";
		const string text2 = "line1\nline2;comment\nline3\n";
		const string text3 = "appended\n";

		fructose_assert_eq(buffer.getBufferSize(), 0);
		buffer.set(text1);
		fructose_assert_eq(buffer.getBufferSize(), text1.length());
		buffer.set(text2);
		fructose_assert_eq(buffer.getBufferSize(), text2.length() - 8);
		buffer.eraseLine();
		fructose_assert_eq(buffer.getBufferSize(), text2.length() - 8 - 6);
		buffer.append(text3);
		fructose_assert_eq(buffer.getBufferSize(), text2.length() - 8 - 6 + text3.length());
		buffer.clear();
		fructose_assert_eq(buffer.getBufferSize(), 0);
	}

	void testMultiLineGet(const string& test_name) {
		GCodeBuffer buffer;
		int32_t rv;
		string rl;
		const string l1nl = "line 1\n", l2nl = "line 2\n", l3nl = "line 3\n";
		const string l1noNl = "line 1", l2noNl = "line 2", l3noNl = "line 3";

		buffer.set(l1nl + l2nl + l3nl);
		fructose_assert_eq(buffer.getBufferedLines(), 3);
		rv = buffer.getNextLine(rl);
		fructose_assert_eq(rv, 1); fructose_assert_eq(rl, l1noNl);
		rv = buffer.getNextLine(rl, 1);
		fructose_assert_eq(rv, 1); fructose_assert_eq(rl, l1noNl);
		rv = buffer.getNextLine(rl, 2);
		fructose_assert_eq(rv, 2); fructose_assert_eq(rl, l1nl + l2noNl);
		rv = buffer.getNextLine(rl, 3);
		fructose_assert_eq(rv, 3); fructose_assert_eq(rl, l1nl + l2nl + l3noNl);
		rv = buffer.getNextLine(rl, 4);
		fructose_assert_eq(rv, 3);

		buffer.set(l1nl + l2nl + l3noNl);
		rv = buffer.getNextLine(rl, 3);
		fructose_assert_eq(rv, 3); fructose_assert_eq(rl, l1nl + l2nl + l3noNl);
	}

	void testMultiLineErase(const string& test_name) {
		GCodeBuffer buffer;
		int32_t rv;
		string rl;
		const string l1nl = "line 1\n", l2nl = "line 2\n", l3nl = "line 3\n";
		const string l1noNl = "line 1", l2noNl = "line 2", l3noNl = "line 3";

		buffer.set(l1nl + l2nl + l3nl);
		fructose_assert_eq(buffer.getBufferedLines(), 3);
		rv = buffer.getNextLine(rl, 3);
		fructose_assert_eq(rv, 3); fructose_assert_eq(rl, l1nl + l2nl + l3noNl);
		rv = buffer.eraseLine(2);
		fructose_assert_eq(rv, 2);
		fructose_assert_eq(buffer.getBufferedLines(), 1);
		rv = buffer.getNextLine(rl, 2);
		fructose_assert_eq(rv, 1);
		rv = buffer.getNextLine(rl);
		fructose_assert_eq(rv, 1); fructose_assert_eq(rl, l3noNl);

		rv = buffer.eraseLine();
		fructose_assert_eq(rv, 1);
		fructose_assert_eq(buffer.getBufferedLines(), 0);
		rv = buffer.eraseLine();
		fructose_assert_eq(rv, 0);
		fructose_assert_eq(buffer.getBufferedLines(), 0);
	}

	void testBucketBoundaries(const string& test_name) {
		fructose_assert_eq(true, false);
		//append() and getNextLine() currently do not operate across bucket boundaries.
	}

	void testMaxBufferSize(const string& test_name) {
		GCodeBuffer buffer;

		int32_t maxSize = buffer.getMaxBufferSize();
		string char1k(1023, '5'); char1k += '\n';

		fructose_assert_eq(buffer.getBufferSize(), 0);
		//almost fill buffer, afterwards we'll have exactly 1k + maxSize % 1024 left
		for (int32_t i = 0; i < (maxSize / 1024) - 1; ++i) {
			fructose_assert_eq(buffer.append(char1k), GCodeBuffer::GSR_OK);
		}
		fructose_assert_eq(buffer.getBufferSize(), maxSize - 1024 - maxSize % 1024);

		string oneTooMany(1024 + maxSize % 1024, 'n'); oneTooMany += '\n';
		string exactlyEnough(1024 + maxSize % 1024 - 1, 'y'); exactlyEnough += '\n';

		fructose_assert_eq(buffer.append(oneTooMany), GCodeBuffer::GSR_BUFFER_FULL);
		fructose_assert_eq(buffer.getBufferSize(), maxSize - 1024 - maxSize % 1024);
		fructose_assert_eq(buffer.append(exactlyEnough), GCodeBuffer::GSR_OK);
		fructose_assert_eq(buffer.getBufferSize(), maxSize);
	}
};

int main(int argc, char** argv) {
	t_GCodeBuffer tests;
	tests.add_test("set", &t_GCodeBuffer::testSet);
	tests.add_test("append", &t_GCodeBuffer::testAppend);
	tests.add_test("appendWithoutNewlines", &t_GCodeBuffer::testAppendWithoutNewlines);
	tests.add_test("seqencedAppend", &t_GCodeBuffer::testAppendConsistencyChecks);
	tests.add_test("getErase", &t_GCodeBuffer::testGetErase);
	tests.add_test("cleanup", &t_GCodeBuffer::testCleanup);
	tests.add_test("lineCounting", &t_GCodeBuffer::testLineCounting);
	tests.add_test("bufferSize", &t_GCodeBuffer::testBufferSize);
	tests.add_test("multiLineGet", &t_GCodeBuffer::testMultiLineGet);
	tests.add_test("multiLineErase", &t_GCodeBuffer::testMultiLineErase);
	//tests.add_test("bucketBoundaries", &t_GCodeBuffer::testBucketBoundaries);
	tests.add_test("maxBufferSize", &t_GCodeBuffer::testMaxBufferSize);
	return tests.run(argc, argv);
}
