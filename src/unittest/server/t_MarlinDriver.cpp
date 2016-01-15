#include <string>
#include <fructose/fructose.h>
#include "../../drivers/MarlinDriver.h"
#include "../../server/Server.h"

using std::string;

struct t_MarlinDriver : public fructose::test_base<t_MarlinDriver>, public MarlinDriver {
	t_MarlinDriver()
	: MarlinDriver(s, "", 0), s("", "")
	{}

	void testTemperatureParsing(const string& test_name) {
		parseTemperatures(*new string("T:19 /0.0 B:0.0 /0.0 @:0 B@:0"));
		fructose_assert_eq(temperature_, 19); fructose_assert_eq(targetTemperature_, 0);
		fructose_assert_eq(bedTemperature_, 0); fructose_assert_eq(targetBedTemperature_, 0);

		parseTemperatures(*new string("ok T:19.9 /0.0 B:0.0 /0.0 @:0 B@:0"));
		fructose_assert_eq(temperature_, 19); fructose_assert_eq(targetTemperature_, 0);
		fructose_assert_eq(bedTemperature_, 0); fructose_assert_eq(targetBedTemperature_, 0);

		parseTemperatures(*new string("ok T:19.0 /180.5 B:0.0 /0.0 @:0 B@:0"));
		fructose_assert_eq(temperature_, 19); fructose_assert_eq(targetTemperature_, 180);

		parseTemperatures(*new string("ok T:19.9 /180.5 B:90.0 /150.9 @:0 B@:0"));
		fructose_assert_eq(temperature_, 19); fructose_assert_eq(targetTemperature_, 180);
		fructose_assert_eq(bedTemperature_, 90); fructose_assert_eq(targetBedTemperature_, 150);

		targetTemperature_ = 5; bedTemperature_ = 25; targetBedTemperature_ = 85;
		parseTemperatures(*new string("ok T:19.1 @:0"));
		fructose_assert_eq(temperature_, 19); fructose_assert_eq(targetTemperature_, 5);
		fructose_assert_eq(bedTemperature_, 25); fructose_assert_eq(targetBedTemperature_, 85);

		parseTemperatures(*new string("ok T:19.1 @:0"));
		parseTemperatures(*new string("ok T:19.1 /0.0 @:0 B:29.0 /0.0 "));
		parseTemperatures(*new string("ok T:19.1 @:0 B:29.0 "));
	}

	void testExtractGCodeInfo(const string& test_name) {
		targetTemperature_ = targetBedTemperature_ = 0;

		//basic tests

		extractGCodeInfo("M109 S220");
		fructose_assert_eq(targetTemperature_, 220);

		extractGCodeInfo("M90\nM91\nM109 S60\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
		fructose_assert_eq(targetTemperature_, 60);

		extractGCodeInfo("M190 S100");
		fructose_assert_eq(targetBedTemperature_, 100);

		extractGCodeInfo("M90\nM91\nM190 S70\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
		fructose_assert_eq(targetBedTemperature_, 70);


		//test that with multiple parseable commands, the last one wins

		extractGCodeInfo("M190 S10\nM109 S25\nM190 S40\nM109 S45");
		fructose_assert_eq(targetTemperature_, 45);
		fructose_assert_eq(targetBedTemperature_, 40);


		//M104/M140 are not parsed

		targetTemperature_ = targetBedTemperature_ = 0;
		extractGCodeInfo("M104 S15\nM140 S32");
		fructose_assert_eq(targetTemperature_, 0);
		fructose_assert_eq(targetBedTemperature_, 0);
	}

private:
	Server s;
};

int main(int argc, char** argv) {
	t_MarlinDriver tests;
	tests.add_test("temperatureParsing", &t_MarlinDriver::testTemperatureParsing);
	tests.add_test("extractGCodeInfo", &t_MarlinDriver::testExtractGCodeInfo);
	return tests.run(argc, argv);
}
