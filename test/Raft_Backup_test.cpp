#include "Config.h"
#include "gtest/gtest.h"

#include <melon/Log.h>

using namespace melon;

namespace cherry {

int milli_raft_election_timeout = 1000;

TEST(raft_agree_test, TestBackup) {
	Logger::setLogLevel(LogLevel::INFO);
	Singleton<Logger>::getInstance()->addAppender("console", LogAppender::ptr(new ConsoleAppender()));

	uint32_t servers = 5;
	Scheduler scheduler;
	scheduler.startAsync();
	Config cfg(servers, &scheduler);
	cfg.start();

	int count = 1;

	cfg.one(std::to_string(count++), servers, true);
	LOG_INFO << "pass 1";

	// put leader and one follower in a partition
	int leader1 = cfg.checkOnLeader();
	cfg.setConnection((leader1 + 2) % servers, false);
	cfg.setConnection((leader1 + 3) % servers, false);
	cfg.setConnection((leader1 + 4) % servers, false);
	LOG_INFO << "put leader and one follower in a partition";

	// submit lots of commands that won't commit
	uint32_t index;
	uint32_t term;
	for (int i = 0; i < 50; ++i) {
		cfg.getRaft(leader1)->start(std::to_string(count++), index, term);
	}

	usleep(milli_raft_election_timeout * 1000 / 2);

	cfg.setConnection((leader1 + 0) % servers, false);
	cfg.setConnection((leader1 + 1) % servers, false);

	// allow other partition to recover
	cfg.setConnection((leader1 + 2) % servers, true);
	cfg.setConnection((leader1 + 3) % servers, true);
	cfg.setConnection((leader1 + 4) % servers, true);
	LOG_INFO << "allow other partition to recover";

	// lots of successful commands to new group.
	for (int i = 0; i < 50; ++i) {
		cfg.one(std::to_string(count++), 3, true);
	}
	LOG_INFO << "pass 2";

	// now another partitioned leader and one follower
	int leader2 = cfg.checkOnLeader();
	int other = (leader1 + 2) % servers;
	if (leader2 == other) {
		other = (leader2 + 1) % servers;
	}
	cfg.setConnection(other, false);

	// lots more commands that won't commit
	for (int i = 0; i < 50; ++i) {
		cfg.getRaft(leader2)->start(std::to_string(count++), index, term);
	}

	usleep(milli_raft_election_timeout * 1000 / 2);

	// bring original leader back to life
	for (uint32_t i = 0; i < servers; ++i) {
		cfg.setConnection(i, false);
	}

	cfg.setConnection(leader1 + 0, true);
	cfg.setConnection(leader1 + 1, true);
	cfg.setConnection(other, true);
	LOG_INFO << "bring original leader back to life";

	// lots of successful commands to new group.
	for (int i = 0; i < 50; ++i) {
		cfg.one(std::to_string(count++), 3, true);
	}
	LOG_INFO << "pass 3";

	// now everyone
	for (uint32_t i = 0; i < servers; ++i) {
		cfg.setConnection(i, true);
	}
	LOG_INFO << "bring all back to life";

	cfg.one(std::to_string(count++), servers, true);
	LOG_INFO << "pass 4";
}

}
