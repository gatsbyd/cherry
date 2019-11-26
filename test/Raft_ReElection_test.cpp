#include "Config.h"
#include "gtest/gtest.h"

#include <melon/Log.h>

using namespace melon;

namespace cherry {

int milli_raft_election_timeout = 1000;

TEST(raft_election_test, TestReElection) {
	Logger::setLogLevel(LogLevel::INFO);
	Singleton<Logger>::getInstance()->addAppender("console", LogAppender::ptr(new ConsoleAppender()));

	uint32_t servers = 3;
	melon::Scheduler scheduler;
	scheduler.startAsync();
	Config cfg(servers, &scheduler);
	cfg.start();

	int leader1 = cfg.checkOnLeader();
	LOG_INFO << "first check one leader passed, leader is " << leader1;

	// if the leader disconnects, a new one should be elected.
	cfg.setConnection(leader1, false);
	int tmp_leader = cfg.checkOnLeader();
	LOG_INFO << "second check one leader passed, leader is " << tmp_leader;

	// if the old leader rejoins, that shouldn't
	// disturb the new leader.
	cfg.setConnection(leader1, true);
	int leader2 = cfg.checkOnLeader();
	LOG_INFO << "3th check one leader passed, leader is " << leader2;

	// if there's no quorum, no leader should
	// be elected.
	cfg.setConnection(leader2, false);
	cfg.setConnection((leader2 + 1) % servers, false);
	usleep(2 * milli_raft_election_timeout * 1000);
	cfg.checkNoLeader();
	LOG_INFO << "4th check no leader passed";

	// if a quorum arises, it should elect a leader.
	cfg.setConnection((leader2 + 1) % servers, true);
	cfg.checkOnLeader();
	LOG_INFO << "5th check one leader passed";

	// re-join of last node shouldn't prevent leader from existing.
	cfg.setConnection(leader2, true);
	cfg.checkOnLeader();
	LOG_INFO << "6th check one leader passed";
}

}
