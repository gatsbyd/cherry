#include "Config.h"
#include "gtest/gtest.h"

namespace cherry {

int milli_raft_election_timeout = 1000;

TEST(raft_test, TestInitialElection) {
	uint32_t servers = 3;
	melon::Scheduler scheduler;
	scheduler.startAsync();
	Config cfg(servers, &scheduler);
	cfg.start();

	cfg.checkOnLeader();

	usleep(50 * 1000);
	uint32_t term1 = cfg.checkTerms();

	//does the leader+term stay the same if there is no network failure?
	usleep(2 * milli_raft_election_timeout * 1000);
	uint32_t term2 = cfg.checkTerms();
	EXPECT_EQ(term1, term2);

	cfg.checkOnLeader();
}

}
