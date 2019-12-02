#include "config.h"
#include "gtest/gtest.h"

#include <melon/Log.h>

using namespace melon;

namespace cherry {

int milli_raft_election_timeout = 1000;

TEST(raft_agree_test, TestFailNoAgree) {
	Logger::setLogLevel(LogLevel::INFO);
	Singleton<Logger>::getInstance()->addAppender("console", LogAppender::ptr(new ConsoleAppender()));

	uint32_t servers = 5;
	Scheduler scheduler;
	scheduler.startAsync();
	Config cfg(servers, &scheduler);
	cfg.start();

	cfg.one("10", servers, false);

	// 3 of 5 followers disconnect
	int leader = cfg.checkOnLeader();
	cfg.setConnection((leader + 1) % servers, false);
	cfg.setConnection((leader + 2) % servers, false);
	cfg.setConnection((leader + 3) % servers, false);
		
	uint32_t term;
	uint32_t index;
	bool is_leader = cfg.getRaft(leader)->start("20", index, term);
	if (!is_leader) {
		LOG_FATAL << "leader rejected Start()";
	}
	if (index != 2) {
		LOG_FATAL << "expected index 2, got " << index;
	}
	usleep(milli_raft_election_timeout * 1000);
	
	int n = cfg.nCommitted(index);
	if (n > 0) {
		LOG_FATAL << n << " committed but no majority";
	}

	// repair
	cfg.setConnection((leader + 1) % servers, true);
	cfg.setConnection((leader + 2) % servers, true);
	cfg.setConnection((leader + 3) % servers, true);

	// the disconnected majority may have chosen a leader from
	// among their own ranks, forgetting index 2.
	int leader2 = cfg.checkOnLeader();
	uint32_t index2;
	uint32_t term2;
	is_leader = cfg.getRaft(leader2)->start("30", index2, term2);
	if (!is_leader) {
		LOG_FATAL << "leader2 rejected Start()";
	}
	if (index2 < 2 || index > 3) {
		LOG_FATAL << "iunexpected index " << index2;
	}

	cfg.one("1000", servers, true);
}

}
