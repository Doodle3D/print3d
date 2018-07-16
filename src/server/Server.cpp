/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <strings.h> /* for error generated by Eclipse on FD_ZERO()? */
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <cstring>
#include "Server.h"
#include "Client.h"
#include "Logger.h"
#include "../settings.h"
#include "../utils.h"
#include "../drivers/DriverFactory.h"

using std::string;

//NOTE: the '##__VA_ARGS__' hack is gnu-specific...no comment on the language *cough*
//FIXME: a better alternative should be created for this
#define LOG(lvl, fmt, ...) log_.log(lvl, "SRV ", fmt, ##__VA_ARGS__)

const bool Server::FORK_BY_DEFAULT = false;
const int Server::SOCKET_MAX_BACKLOG = 5; //private
const int Server::SELECT_LOG_FAST_LOOP = -1;

Server::Server(const string& serialPortPath, const string& socketPath, const string& printerName) :
		socketPath_(socketPath), log_(Logger::getInstance()), socketFd_(-1), printerDriver_(0)
{
	if (!settings_init()) LOG(Logger::ERROR, "could not initialize uci settings context");

	const char *uci_key = "wifibox.general.printer_type";
	const char *printerType;

	if (printerName.empty()) {
		printerType = settings_get(uci_key);
		if (printerType)
			LOG(Logger::INFO, "printer type from settings: '%s'", printerType);
		else
			LOG(Logger::ERROR, "could not retrieve printer type from settings");
	} else {
		printerType = printerName.c_str();
		LOG(Logger::INFO, "printer type from command line: '%s'", printerType);
	}

	if (printerType) {
		printerDriver_ = DriverFactory::createDriver(printerType, *this, serialPortPath, 250000);

		if (!printerDriver_)
			LOG(Logger::ERROR, "no printer driver found for type '%s'", printerType);
	}

	driverDelay = 0;
}

Server::~Server() {
	closeSocket();
	settings_deinit();
}

//returns -1 on error, >0 after fork or 0 after successful (not-fork) exit
int Server::start(bool fork) {
	if (socketFd_ >= 0) {
		LOG(Logger::WARNING, "printserver already started");
		return 0; //TODO: do we need a better return value in this case?
	}

	LOG(Logger::INFO, "starting printserver with socket path '%s'", socketPath_.c_str());

	if (!openSocket()) return false;

	//NOTE: ignore SIGPIPE, generated by send() when the other end closes the socket (see Beej's guide for details)
	//FIXME: replace this with sigaction()
	signal(SIGPIPE, SIG_IGN);

	if (fork) {
		//fork as late as possible (before any possibly lengthy operations like serial port things)
		int rv = forkProcess();
		if (rv < 0) {
			closeSocket();
		}

		if (rv != 0)
			return rv; //return rv unless we are a successfuly spawned child process
	}

	if (printerDriver_) printerDriver_->openConnection();

	fd_set masterFds;
	fd_set readFds;
	int maxFd = socketFd_;
	FD_ZERO(&masterFds);
	FD_SET(socketFd_, &masterFds);

	bool timeoutEnabled = true;
	struct timeval timeout = (struct timeval ) { 0, 0 };
	struct timeval startTime, endTime, diffTime;
	while (true) {
		readFds = masterFds;
		for (set_int::const_iterator it = registeredFds_.begin();
				it != registeredFds_.end(); ++it) {
			FD_SET(*it, &readFds);
		}
		::gettimeofday(&startTime, NULL);

		//LOG(Logger::BULK, "entering select(), maxfd=%i", maxFd);
		if (log_.checkError(
				::select(maxFd + 1, &readFds, NULL, NULL,
						timeoutEnabled ? &timeout : NULL), /* use FD_SETSIZE instead of keeping maxfd? */
				"SRV ", "error in select()")) {
			//TODO: handle error (close down server <- needs function... and return with proper error value)
		}
		::gettimeofday(&endTime, NULL);
		timeval_subtract(&diffTime, &endTime, &startTime);
		long diffMillis = diffTime.tv_sec * 1000 + diffTime.tv_usec / 1000;

		if (diffMillis <= SELECT_LOG_FAST_LOOP) {
			LOG(Logger::BULK, "returned from select() after %li ms",
					diffMillis);
		}

		if (FD_ISSET(socketFd_, &readFds)) {
			//TODO: move socket setup to client init
			socklen_t len = sizeof(struct sockaddr_un);
			struct sockaddr_un peerAddr;
			int connFd = ::accept(socketFd_, (struct sockaddr*) &peerAddr,
					&len);
			int flags = ::fcntl(connFd, F_GETFL, 0);
			log_.checkError(flags, "SRV ", "could not obtain flags for socket with fd ", connFd);
			log_.checkError(::fcntl(connFd, F_SETFL, (flags | O_NONBLOCK)), "SRV ",
					"could not enable non-blocking mode on socket with fd ", connFd);

			clients_.push_back(new Client(*this, connFd));
			//LOG(Logger::BULK, "new client with fd %i", connFd);

			maxFd = (connFd > maxFd ? connFd : maxFd);
			FD_SET(connFd, &masterFds);
		}

		for (vec_ClientP::iterator it = clients_.begin(); it != clients_.end();
		/* increment inside loop */) {
			Client* client = *it;
			int rv = client->readData();
			log_.checkError(rv, "SRV ", "cannot read from client");

			if (rv >= 0 || (rv == -2 && client->getBufferSize() > 0)) {
				if (rv >= 0) {
					//LOG(Logger::BULK, "read %i bytes from client with fd %i", rv, client->getFileDescriptor());
				} else {
					LOG(Logger::WARNING, "client with fd %i closed connection, still %i bytes available", client->getFileDescriptor(), client->getBufferSize());
				}

				client->runCommands();
			}

			if (rv == -2) {
				//LOG(Logger::BULK, "connection closed from client with fd %i", client->getFileDescriptor());
				::close(client->getFileDescriptor());
				FD_CLR(client->getFileDescriptor(), &masterFds);
				delete (client);
				it = clients_.erase(it);
			} else {
				++it;
			}
		}

		if (printerDriver_) {
			int newTimeout = printerDriver_->update();
			timeoutEnabled = (newTimeout >= 0) ? true : false;

			timeout.tv_sec = newTimeout / 1000;
			timeout.tv_usec = (newTimeout % 1000) * 1000;
		} else {
			timeoutEnabled = false;
		}
	}

	return 0;
}


bool Server::registerFileDescriptor(int fd) {
	std::pair<set_int::iterator, bool> rv = registeredFds_.insert(fd);
	return rv.second;
}

bool Server::unregisterFileDescriptor(int fd) {
	return (registeredFds_.erase(fd) == 1);
}


void Server::cancelAllTransactions(const Client *exclude) {
	if (exclude) LOG(Logger::VERBOSE, "cancelling all transactions except for fd %i (#clients: %i)", exclude->getFileDescriptor(), clients_.size());
	else LOG(Logger::VERBOSE, "cancelling all transactions (#clients: %i)", clients_.size());

	for (vec_ClientP::iterator it = clients_.begin(); it != clients_.end(); it++) {
		if (exclude == *it) continue;
		Client::Transaction &trx = (*it)->getTransaction();
		trx.cancelled = true;
	}
}

AbstractDriver* Server::getDriver() {
	return printerDriver_;
}

const AbstractDriver* Server::getDriver() const {
	return printerDriver_;
}


bool Server::requestExit(int rv) {
	LOG(Logger::INFO, "server exiting (rv=%i)", rv);
	closeSocket();
	exit(rv);
}

/*********************
 * PRIVATE FUNCTIONS *
 *********************/

bool Server::openSocket() {
	int rv;

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path));

	socketFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
	if (log_.checkError(socketFd_, "SRV ", "could not create domain socket"))
		return false;

	rv = unlink(addr.sun_path); //FIXME (corner case): this does look a bit dangerous in case of funky paths, check if it is a socket first?
	if (rv == -1 && errno != ENOENT && errno != ENOTDIR) /* ignore errors indicating the socket file did not exist */
		if (log_.checkError(rv, "SRV ", "could not unlink domain socket file"))
			return false;

	rv = bind(socketFd_, (struct sockaddr*) &addr, sizeof(sockaddr_un)); //what is up with the address_len parameter value? and SUN_LEN()?
	if (log_.checkError(rv, "SRV ", "could not bind domain socket"))
		return false;

	rv = listen(socketFd_, SOCKET_MAX_BACKLOG);
	if (log_.checkError(rv, "SRV ", "could not listen on domain socket"))
		return false;

	return true;
}

bool Server::closeSocket() {
	if (log_.checkError(close(socketFd_), "SRV ", "could not close domain socket"))
		return -1;
	socketFd_ = -1;

	return true;
}

//returns 0 to child, >0 to parent, -1 for error
int Server::forkProcess() {
	int rv;

	int child_pid = fork();
	switch (child_pid) {
	case -1:	//this is an error (no child created at all)
		log_.checkError(child_pid, "SRV ", "could not fork server");
		return -1;
		break;
	case 0:	//this is the child
		//TODO: setsid() should probably disabled if the server also spawns its own sub-servers
		rv = setsid();
		if (rv == -1) { //try to create a new session for the child so it's not a child anymore
			log_.checkError(rv, "SRV ", "could not create new session for forked process");
			return -1;
		}
		return 0;
		break;
	default: //this is the parent with child_pid being the pid of the new child
		LOG(Logger::INFO, "printserver forked to background. pid=%i\n", child_pid);
		return child_pid;
	}
}
