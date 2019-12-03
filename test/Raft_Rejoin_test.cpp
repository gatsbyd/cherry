#include "Config.h"
#include "gtest/gtest.h"

#include <melon/Log.h>

using namespace melon;

namespace cherry {

TEST(raft_agree_test, TestRejoin) {
	Logger::setLogLevel(LogLevel::INFO);
	Singleton<Logger>::getInstance()->addAppender("console", LogAppender::ptr(new ConsoleAppender()));

	uint32_t servers = 3;
	Scheduler scheduler;
	scheduler.startAsync();
	Config cfg(servers, &scheduler);
	cfg.start();

	cfg.one("101", servers, true);
	// leader network failure
	int leader1 = cfg.checkOnLeader();
	cfg.setConnection(leader1, false);

	// make old leader try to agree on some entries
	uint32_t index;
	uint32_t term;
	cfg.getRaft(leader1)->start("102", index, term);
	cfg.getRaft(leader1)->start("103", index, term);
	cfg.getRaft(leader1)->start("104", index, term);

	// new leader commits, also for index=2
	cfg.one("103", 2, true);

	// new leader network failure
	int leader2 = cfg.checkOnLeader();
	cfg.setConnection(leader2, false);

	// old leader connected again
	cfg.setConnection(leader1, true);

	cfg.one("104", 2, true);

	// all together now
	cfg.setConnection(leader2, true);

	cfg.one("105", servers, true);
}

}
