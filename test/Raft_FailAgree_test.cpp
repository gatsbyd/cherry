#include "Config.h"
#include "gtest/gtest.h"

#include <melon/Log.h>

using namespace melon;

namespace cherry {

int milli_raft_election_timeout = 1000;

TEST(raft_agree_test, TestFailAgree) {
	Logger::setLogLevel(LogLevel::INFO);
	Singleton<Logger>::getInstance()->addAppender("console", LogAppender::ptr(new ConsoleAppender()));

	uint32_t servers = 3;
	Scheduler scheduler;
	scheduler.startAsync();
	Config cfg(servers, &scheduler);
	cfg.start();	

	cfg.one("101", servers, false);

	// follower network disconnection
	int leader = cfg.checkOnLeader();
	cfg.setConnection((leader + 1) % servers, false);

	// agree despite one disconnected server?
	cfg.one("102", servers - 1, false);
	cfg.one("103", servers - 1, false);
	usleep(milli_raft_election_timeout * 1000);
	cfg.one("104", servers - 1, false);
	cfg.one("105", servers - 1, false);

	// re-connect
	cfg.setConnection((leader + 1) % servers, true);

	// agree with full set of servers?
	usleep(milli_raft_election_timeout * 1000);
	cfg.one("106", servers, false);
	usleep(milli_raft_election_timeout * 1000);
	cfg.one("107", servers, false);
}

}
