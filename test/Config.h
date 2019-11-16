#ifndef _CHERRY_CONFIG_H_
#define _CHERRY_CONFIG_H_

#include "../src/Raft.h"

#include <vector>
#include <map>

namespace cherry {

class Raft;

class Config {
public:
	Config(uint32_t n, melon::Scheduler* scheduler);
	void start();
	void setConnection(uint32_t idx, bool connection);

	uint32_t checkOnLeader();
	uint32_t checkTerms();

private:
	std::shared_ptr<Raft> makeRaft(uint32_t idx, uint32_t n, melon::Scheduler* scheduler);
	
private:
	uint32_t n_;
	const std::string ip_;
	int base_port_;
	std::vector<std::shared_ptr<Raft>> rafts_;
	std::vector<bool> raft_connected_;
 	std::map<int, std::vector<PolishedRpcClient::Ptr> > connections_;
	
};

}
#endif
