#include <string>
#include <fructose/fructose.h>
#include "../../drivers/GCodeBuffer.h"

using std::string;

struct t_GCodeBuffer : public fructose::test_base<t_GCodeBuffer> {
	void testSet(const std::string& test_name) {
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

	void testAppend(const std::string& test_name) {
		GCodeBuffer buffer;

		buffer.set("abc\n");
		buffer.append("xyz\nnext line");
		fructose_assert_eq(buffer.getTotalLines(), 3);

		buffer.clear();
		buffer.append("def\nmore data\n");
		fructose_assert_eq(buffer.getTotalLines(), 2);
	}

	void testGetErase(const std::string& test_name) {
		GCodeBuffer buffer;

		const string line1 = "line 1", line2 = "line 2", line3 = "line 3", line4 = "line 4";
		buffer.append(line1 + "\n" + line2 + "\n" + line3 + "\n" + line4);
		fructose_assert_eq(buffer.getTotalLines(), 4);

		string lineBuf;
		bool rv;

		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(lineBuf, line1);
		rv = buffer.getNextLine(lineBuf);
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

	void testCleanup(const std::string& test_name) {
		GCodeBuffer buffer;
		string lineBuf;
		bool rv;

		buffer.set("line   ;  with comment");
		fructose_assert_eq(buffer.getTotalLines(), 1);
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, true);
		fructose_assert_eq(lineBuf, "line   ");

		buffer.set("car+new mix\r\n\r\n\n\n\n");
		fructose_assert_eq(buffer.getTotalLines(), 1);
		rv = buffer.getNextLine(lineBuf);
		fructose_assert_eq(rv, true);
		fructose_assert_eq(lineBuf, "car+new mix");
	}

	void testLineCounting(const std::string& test_name) {
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
};

int main(int argc, char** argv) {
	t_GCodeBuffer tests;
	tests.add_test("set", &t_GCodeBuffer::testSet);
	tests.add_test("append", &t_GCodeBuffer::testAppend);
	tests.add_test("getErase", &t_GCodeBuffer::testGetErase);
	tests.add_test("cleanup", &t_GCodeBuffer::testCleanup);
	tests.add_test("lineCounting", &t_GCodeBuffer::testLineCounting);
	return tests.run(argc, argv);
}
