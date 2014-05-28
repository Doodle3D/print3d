#include <string>
#include <fructose/fructose.h>
#include "../../drivers/GCodeBuffer.h"

using std::string;

struct t_GCodeBuffer : public fructose::test_base<t_GCodeBuffer> {
	void testSet(const string& test_name) {
		GCodeBuffer buffer;

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

	void testGetErase(const string& test_name) {
		GCodeBuffer buffer;

		const string line1 = "line 1", line2 = "line 2", line3 = "line 3", line4 = "line 4";
		buffer.append(line1 + "\n" + line2 + "\n" + line3 + "\n" + line4);
		fructose_assert_eq(buffer.getTotalLines(), 4);

		string lineBuf;
		bool rv;

		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, true);
		fructose_assert_eq(lineBuf, line1);
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, true);
		fructose_assert_eq(lineBuf, line1);

		buffer.eraseLine();
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, true);
		fructose_assert_eq(lineBuf, line2);

		buffer.eraseLine();
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, true);
		fructose_assert_eq(lineBuf, line3);

		buffer.eraseLine();
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, true);
		fructose_assert_eq(lineBuf, line4);

		buffer.eraseLine();
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, false);
		fructose_assert_eq(lineBuf, "");
	}

	void testCleanup(const string& test_name) {
		GCodeBuffer buffer;
		string lineBuf;
		bool rv;

		buffer.set("line   ;  with comment");
		fructose_assert_eq(buffer.getTotalLines(), 1);
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, true);
		fructose_assert_eq(lineBuf, "line   ");

		buffer.set(";  pure comment");
		fructose_assert_eq(buffer.getTotalLines(), 0);
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, false);

		buffer.set("car+new mix\r\n\r\n\n\n\n");
		fructose_assert_eq(buffer.getTotalLines(), 1);
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, true);
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
		bool rv;
		string rl;
		const string l1nl = "line 1\n", l2nl = "line 2\n", l3nl = "line 3\n";
		const string l1noNl = "line 1", l2noNl = "line 2", l3noNl = "line 3";

		buffer.set(l1nl + l2nl + l3nl);
		rv = buffer.getNextLine(rl);
		fructose_assert_eq(rv, true); fructose_assert_eq(rl, l1noNl);
		rv = buffer.getNextLine(rl, 1);
		fructose_assert_eq(rv, true); fructose_assert_eq(rl, l1noNl);
		rv = buffer.getNextLine(rl, 2);
		fructose_assert_eq(rv, true); fructose_assert_eq(rl, l1nl + l2noNl);
		rv = buffer.getNextLine(rl, 3);
		fructose_assert_eq(rv, true); fructose_assert_eq(rl, l1nl + l2nl + l3noNl);
		rv = buffer.getNextLine(rl, 4);
		fructose_assert_eq(rv, false);

		buffer.set(l1nl + l2nl + l3noNl);
		rv = buffer.getNextLine(rl, 3);
		fructose_assert_eq(rv, true); fructose_assert_eq(rl, l1nl + l2nl + l3noNl);
	}

	void testMultiLineErase(const string& test_name) {
		GCodeBuffer buffer;
		bool rv;
		string rl;
		const string l1nl = "line 1\n", l2nl = "line 2\n", l3nl = "line 3\n";
		const string l1noNl = "line 1", l2noNl = "line 2", l3noNl = "line 3";

		buffer.set(l1nl + l2nl + l3nl);
		rv = buffer.getNextLine(rl, 3);
		fructose_assert_eq(rv, true); fructose_assert_eq(rl, l1nl + l2nl + l3noNl);
		rv = buffer.eraseLine(2);
		fructose_assert_eq(rv, true);
		rv = buffer.getNextLine(rl, 2);
		fructose_assert_eq(rv, false);
		rv = buffer.getNextLine(rl);
		fructose_assert_eq(rv, true); fructose_assert_eq(rl, l3noNl);

		rv = buffer.eraseLine();
		fructose_assert_eq(rv, true);
		rv = buffer.eraseLine();
		fructose_assert_eq(rv, false);
	}

	void testBucketBoundaries(const string& test_name) {
		fructose_assert_eq(true, false);
		//append() and getNextLine() currently do not operate across bucket boundaries.
	}
};

int main(int argc, char** argv) {
	t_GCodeBuffer tests;
	tests.add_test("set", &t_GCodeBuffer::testSet);
	tests.add_test("append", &t_GCodeBuffer::testAppend);
	tests.add_test("getErase", &t_GCodeBuffer::testGetErase);
	tests.add_test("cleanup", &t_GCodeBuffer::testCleanup);
	tests.add_test("lineCounting", &t_GCodeBuffer::testLineCounting);
	tests.add_test("bufferSize", &t_GCodeBuffer::testBufferSize);
	tests.add_test("multiLineGet", &t_GCodeBuffer::testMultiLineGet);
	tests.add_test("multiLineErase", &t_GCodeBuffer::testMultiLineErase);
	//tests.add_test("bucketBoundaries", &t_GCodeBuffer::testBucketBoundaries);
	return tests.run(argc, argv);
}
