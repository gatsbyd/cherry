#include "Config.h"
#include "gtest/gtest.h"

#include <melon/Log.h>
#include <unistd.h>

namespace cherry {

Config::Config(uint32_t n, melon::Scheduler* scheduler)
	:n_(n),
	ip_("127.0.0.1"),
	base_port_(5000),
	raft_connected_(n, true) {

	for (uint32_t i = 0; i < n; ++i) {
		rafts_.push_back(makeRaft(i, n, scheduler));
	}

}

std::shared_ptr<Raft> Config::makeRaft(uint32_t idx, uint32_t n, melon::Scheduler* scheduler) {
	std::vector<PolishedRpcClient::Ptr> peers;
	for (uint32_t j = 0; j < n; ++j) {
		melon::IpAddress server_addr(ip_, base_port_ + j);
		peers.push_back(std::make_shared<PolishedRpcClient>(server_addr, scheduler));
	}
	connections_[idx] = peers;

	melon::IpAddress addr(base_port_ + idx);
	std::shared_ptr<Raft> raft = std::make_shared<Raft>(peers, idx, addr, scheduler);
	return raft;
}

void Config::start() {
	for (const auto& raft : rafts_) {
		raft->start();
	}
}

void Config::setConnection(uint32_t idx, bool connection) {
	raft_connected_[idx] = connection;
	//outgoing
	const std::vector<PolishedRpcClient::Ptr>& peers = connections_[idx];
	for (const PolishedRpcClient::Ptr& peer : peers) {
		peer->setConnected(connection);
	}
	
	//incoming
	std::map<int, std::vector<PolishedRpcClient::Ptr> >::iterator it = connections_.begin();
	while (it != connections_.end()) {
		it->second[idx]->setConnected(connection);
		++it;
	}
}

//check have one and only one leader in a term
uint32_t Config::checkOnLeader() {
	for (int iter = 0; iter < 10; ++iter) {
		int ms = 450 + rand() % 100;
		usleep(ms * 1000);
		
		std::map<uint32_t, std::vector<uint32_t> > leaders;
		for (uint32_t idx = 0; idx < n_; ++idx) {
			if (raft_connected_[idx]) {
				bool is_leader = rafts_[idx]->isLeader();
				uint32_t term = rafts_[idx]->term();
				if (is_leader) {
					leaders[term].push_back(idx);
				}
			}
		}
		uint32_t last_term_with_leader = -1;
		for (const auto& pair : leaders) {
			EXPECT_EQ(1, static_cast<int>(pair.second.size()));
			if (pair.first > last_term_with_leader) {
				last_term_with_leader = pair.first;
			}
		}
		if (leaders.size() != 0) {
			return leaders[last_term_with_leader][0];
		}
	}
	EXPECT_TRUE(false);
	return -1;
}

uint32_t Config::checkTerms() {
	uint32_t term = 0;
	for (uint32_t idx = 0; idx < n_; ++idx) {
		if (raft_connected_[idx]) {
			uint32_t xterm = rafts_[idx]->term();
			if (term == 0) {
				term = xterm;
			} else {
				EXPECT_EQ(term, xterm);
			}

		}
	}
	
	return term;
}

}
