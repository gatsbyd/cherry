#include "Config.h"
#include "gtest/gtest.h"

#include <melon/Log.h>

using namespace melon;

namespace cherry {

int milli_raft_election_timeout = 1000;

TEST(raft_test, TestInitialElection) {
	Logger::setLogLevel(LogLevel::INFO);
	Singleton<Logger>::getInstance()->addAppender("console", LogAppender::ptr(new ConsoleAppender()));

	uint32_t servers = 3;
	melon::Scheduler scheduler;
	scheduler.startAsync();
	Config cfg(servers, &scheduler);
	cfg.start();

	getchar();

	/**
	cfg.checkOnLeader();
	LOG_INFO << "1";

	usleep(50 * 1000);
	uint32_t term1 = cfg.checkTerms();
	LOG_INFO << "2";

	//does the leader+term stay the same if there is no network failure?
	usleep(2 * milli_raft_election_timeout * 1000);
	uint32_t term2 = cfg.checkTerms();
	EXPECT_EQ(term1, term2);
	LOG_INFO << "3";

	cfg.checkOnLeader();
	LOG_INFO << "4";
	**/

}

}
