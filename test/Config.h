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

	int checkOnLeader();
	void checkNoLeader();
	uint32_t checkTerms();
	int nCommitted(int index);
	int one(MessagePtr cmd, int expected_server, bool retry);

private:
	std::shared_ptr<Raft> makeRaft(uint32_t idx, uint32_t n, melon::Scheduler* scheduler);
	void applyFunc(uint32_t, LogEntry);
	
private:
	uint32_t n_;
	const std::string ip_;
	int base_port_;
	std::vector<std::shared_ptr<Raft>> rafts_;
	std::vector<bool> raft_connected_;
 	std::map<int, std::vector<PolishedRpcClient::Ptr> > connections_;
	
	std::map<int, std::vector<LogEntry>>  logs_;
};

}
#endif
