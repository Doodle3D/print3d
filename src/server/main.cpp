/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <cstring>
#include "../ipc_shared.h"
#include "../settings.h"
#include "../drivers/DriverFactory.h"
#include "Logger.h"
#include "Server.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

//NOTE: the '##__VA_ARGS__' hack is gnu-specific...no comment on the language *cough*
//FIXME: a better alternative should be created for this
#define LOG(lvl, fmt, ...) log.log(lvl, "MAIN", fmt, ##__VA_ARGS__)

static const char* UCI_KEY_LOG_PATH = "wifibox.system.log_path";
static const char* UCI_KEY_LOG_BASENAME = "wifibox.system.p3d_log_basename";
static const char* UCI_KEY_LOG_LEVEL = "wifibox.general.system_log_level";

static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"quiet", no_argument, NULL, 'q'},
		{"verbose", no_argument, NULL, 'v'},
		{"fork", no_argument, NULL, 'f'},
		{"no-fork", no_argument, NULL, 'F'},
		{"force", no_argument, NULL, 'S'},
		{"device", required_argument, NULL, 'd'},
		{"printer", required_argument, NULL, 'p'},
		{"use-settings", required_argument, NULL, 'u'},
		{NULL, 0, NULL, 0}
};


int main(int argc, char** argv) {
	string serialDevice = "";
	string printerName = "";
	int doFork = 0; //-1: don't fork, 0: leave default, 1: do fork
	bool showHelp = false, forceStart = false;
	Logger::ELOG_LEVEL logLevel = Logger::WARNING;
	bool logLevelFromCmdLine = false;
	bool useUci = false;
	int ch;

	while ((ch = getopt_long(argc, argv, "hqvfFSd:p:u", long_options, NULL)) != -1) {
		switch (ch) {
			case 'h': showHelp = true; break;
			case 'q':
				if (logLevel > Logger::ERROR) logLevel = Logger::ERROR;
				else if (logLevel > Logger::QUIET) logLevel = Logger::QUIET;

				logLevelFromCmdLine = true;
				break;
			case 'v':
				if (logLevel < Logger::VERBOSE) logLevel = Logger::VERBOSE;
				else if (logLevel < Logger::BULK) logLevel = Logger::BULK;

				logLevelFromCmdLine = true;
				break;
			case 'f': doFork = 1; break;
			case 'F': doFork = -1; break;
			case 'S': forceStart = true; break;
			case 'd': serialDevice = optarg; break;
			case 'p': printerName = optarg; break;
			case 'u': useUci = true; break;

			case ':': case '?':
				::exit(1);
		}
	}

	printf("Print 3D server\n");

	/* show some basic information on the platform type */
	printf("Platform information: ");
#ifdef __BIG_ENDIAN__
	printf("big endian");
#elif __LITTLE_ENDIAN__
	printf("little endian");
#else
	printf("unknown endianness (assuming big)");
#endif
	printf(", sizeof(short)=%zu, sizeof(int)=%zu, sizeof(long)=%zu, sizeof(float)=%zu\n", sizeof(short), sizeof(int), sizeof(long), sizeof(float));

	if (showHelp) {
		printf("The following options are accepted (forking is %s by default):\n", Server::FORK_BY_DEFAULT ? "on" : "off");
		printf("\t-h,--help\t\tDisplay this help message\n");
		printf("\t-q,--quiet\t\tLog only errors (repeat to disable logging)\n");
		printf("\t-v,--verbose\t\tLog verbose (repeat for bulk; default: log only errors and warnings)\n");
		printf("\t-f,--fork\t\tFork the server process\n");
		printf("\t-F,--no-fork\t\tDo not fork the server process\n");
		printf("\t-S,--force\t\tForce starting the server with a default device if none could be found\n");
		printf("\t-d,--device\t\tThe printer serial device to use (any prefix path will be cut off)\n");
		printf("\t-p,--printer\t\tThe 3D printer driver to use (use help to get more information)\n");
		printf("\t-u,--use-settings\tRead log target and level from (UCI) settings\n");
		::exit(0);
	}

	//handle the '-p help' case before looking for devices
	if (printerName.compare("help") == 0) {
		DriverFactory::vec_DriverInfoP infos = DriverFactory::getDriverInfo();
		cout << "Listing supported printer drivers and device models they support." << endl;

		for (DriverFactory::vec_DriverInfoP::const_iterator d_it = infos.begin();
				d_it != infos.end(); ++d_it) {
			const AbstractDriver::DriverInfo* di = *d_it;
			cout << "* Models supported by driver '" << di->name << "':";

			for (AbstractDriver::vec_FirmwareDescription::const_iterator m_it = di->supportedFirmware.begin();
					m_it != di->supportedFirmware.end(); ++m_it) {
				cout << " " << (*m_it).name;
			}
			cout << "." << endl;
		}

		::exit(0);
	}


	/* First set up logging. */

	Logger& log = Logger::getInstance();

	if (useUci && settings_available()) {
		if (!settings_init()) cerr << "could not initialize uci settings context";

		//TODO: the difference between not_found and 'actual' errors should be detectable
		const char *logFilePath = settings_get(UCI_KEY_LOG_PATH);
		const char *logBasename = settings_get(UCI_KEY_LOG_BASENAME);
		const char *lvlName = settings_get(UCI_KEY_LOG_LEVEL);
		const Logger::ELOG_LEVEL uciLogLevel = Logger::getLevelForName(lvlName, Logger::VERBOSE);

		if (!logBasename) {
			char *msg = NULL;
			settings_get_error(&msg);
			cerr << "Error: could not read log basename from settings (" << msg << ")" << endl;
			free(msg);
		}

		if (!logLevelFromCmdLine) logLevel = uciLogLevel;
		int rv = log.openParameterized(logFilePath, logBasename, serialDevice.c_str(), logLevel);

		if (rv == -1) cerr << "Error: could not open log" <<
				(logLevelFromCmdLine ? "" : " with UCI settings") <<
				" (" << strerror(errno) << ")" << endl;
		else if (rv == -2) cerr << "Error: could not open log" <<
				(logLevelFromCmdLine ? "" : " with UCI settings") <<
				endl;
	} else if (useUci) { //requested to use settings, but they are not available
		cerr << "Error: cannot read log configuration, settings not available." << endl;
		::exit(1);
	} else {
		log.open(stderr, logLevel);
	}


	/* Find out which serial device to use. */

	if (serialDevice.empty()) {
		char **devlist = ipc_find_devices();

		if (!devlist) { //no list
			LOG(Logger::ERROR, "could not retrieve device list (%s)", strerror(errno));
			::exit(1);
		}

		if (!devlist[0] && !forceStart) { //no devices and no force-start
			LOG(Logger::ERROR, "no devices found, please connect a printer or re-run with '-S'");
			::exit(1);
		} else if (!devlist[0]) { //no devices but force-start requested
			serialDevice = IPC_DEFAULT_DEVICE_ID;
		} else if (devlist[1] != 0) { //more than one device
			LOG(Logger::ERROR, "more than one device found (listed below), please specify one of the following:");
			for (int i = 0; devlist[i] != 0; i++) {
				const char *item = devlist[i];
				LOG(Logger::ERROR, "  '%s'", item);
			}
			::exit(1);
		} else {
			serialDevice = devlist[0];
		}

		ipc_free_device_list(devlist);
	} else {
		size_t lastSlash = serialDevice.rfind('/');
		if (lastSlash != string::npos) serialDevice = serialDevice.substr(lastSlash + 1);
	}


	/* Log some information before starting the actual server. */

	LOG(Logger::INFO, "starting Print3D server");
	LOG(Logger::INFO, "using printer device: '%s'", serialDevice.c_str());

	if (!printerName.empty())
		LOG(Logger::INFO, "using printer type: '%s'", printerName.c_str());
	else
		LOG(Logger::INFO, "using printer type from UCI configuration");

	LOG(Logger::INFO, "using log level: %s", LOG_LEVEL_NAMES[logLevel]);


	Server s("/dev/" + serialDevice, ipc_construct_socket_path(serialDevice.c_str()), printerName);

	if (!s.getDriver()) {
		LOG(Logger::ERROR, "could not create printer driver");
		::exit(1);
	}

	int rv;

	if (doFork == 0) rv = s.start();
	else rv = s.start((doFork == 1) ? true : false);

	if (rv >= 0) ::exit(0);
	else ::exit(1);
}
