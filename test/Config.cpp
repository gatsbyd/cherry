#include "Config.h"

namespace cherry {

Config::Config(uint32_t n, melon::Scheduler* scheduler)
	:n_(n),
	ip_("127.0.0.1"),
	base_port_(5000) {

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

	melon::IpAddress addr(ip_, base_port_ + idx);
	std::shared_ptr<Raft> raft = std::make_shared<Raft>(peers, addr, scheduler);
	return raft;
}

void Config::disconnection(uint32_t idx) {

}

void Config::connection(uint32_t idx) {

}

uint32_t Config::checkOnLeader() {

}

uint32_t Config::checkTerms() {

}

}
