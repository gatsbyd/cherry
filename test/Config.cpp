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
	//Figure 2表明logs下标从1开始，所以添加一个空的Entry
	LogEntry entry;
	entry.set_term(0);
	entry.set_index(0);
	for (uint32_t i = 0; i < n; ++i) {
		logs_[i].push_back(entry);
	}

}

std::shared_ptr<Raft> Config::makeRaft(uint32_t idx, uint32_t n, melon::Scheduler* scheduler) {
	std::vector<PolishedRpcClient::Ptr> peers;
	for (uint32_t j = 0; j < n; ++j) {
		melon::IpAddress server_addr(ip_, base_port_ + j);
		peers.push_back(std::make_shared<PolishedRpcClient>(server_addr, scheduler));
	}

	melon::IpAddress addr(base_port_ + idx);
	std::shared_ptr<Raft> raft = std::make_shared<Raft>(peers, idx, addr, scheduler);
	raft->setApplyFunc(std::bind(&Config::applyFunc, this, std::placeholders::_1, std::placeholders::_2));
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
	const std::vector<PolishedRpcClient::Ptr>& peers = rafts_[idx]->getPeers();
	for (uint32_t i = 0; i < peers.size(); ++i) {
		const PolishedRpcClient::Ptr& peer = peers[i];
		peer->setConnected(connection && raft_connected_[i]);
	}
	
	//incoming
	for (uint32_t i = 0; i < n_; ++i) {
		const std::vector<PolishedRpcClient::Ptr>& peers = rafts_[i]->getPeers();
		peers[idx]->setConnected(connection && raft_connected_[i]);
	}
	LOG_INFO << (connection ? "connect " : "disconnect ") << idx;
}

std::shared_ptr<Raft> Config::getRaft(int index) {
	return rafts_[index];
}

//check have one and only one leader in a term
int Config::checkOnLeader() {
	//TODO:操作rafts_不是线程安全的
	for (int iter = 0; iter < 10; ++iter) {
		int ms = 450 + rand() % 100;
		usleep(ms * 1000);
		
		std::map<uint32_t, std::vector<uint32_t> > term_2_leaderid;
		for (uint32_t idx = 0; idx < n_; ++idx) {
			if (raft_connected_[idx]) {
				bool is_leader = rafts_[idx]->isLeader();
				uint32_t term = rafts_[idx]->term();
				if (is_leader) {
					term_2_leaderid[term].push_back(idx);
				}
			}
		}
		uint32_t last_term_with_leader = 0;
		for (const auto& pair : term_2_leaderid) {
			EXPECT_EQ(1, static_cast<int>(pair.second.size()));
			if (pair.first > last_term_with_leader) {
				last_term_with_leader = pair.first;
			}
		}
		if (term_2_leaderid.size() != 0) {
			return static_cast<int>(term_2_leaderid[last_term_with_leader][0]);
		}
	}
	EXPECT_TRUE(false);
	return -1;
}

// check that there's no leader
void Config::checkNoLeader() {
	for (uint32_t idx = 0; idx < n_; ++idx) {
		if (raft_connected_[idx]) {
			bool is_leader = rafts_[idx]->isLeader();
			EXPECT_TRUE(!is_leader);
		}
	}
}

uint32_t Config::checkTerms() {
	uint32_t term = 0;
	//TODO:锁
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

void Config::applyFunc(uint32_t server_id, LogEntry entry) {
	u_int32_t index = entry.index();
	{
		melon::MutexGuard lock(mutex_);
		for (uint32_t i = 0; i < n_; ++i) {
			if (logs_[i].size() > index) {
				const LogEntry& old = logs_[i][index];
				EXPECT_EQ(old.command(), entry.command());
			}
		}
		logs_[server_id].push_back(entry);
		LOG_INFO << server_id << " apply <index=" << index << ", cmd=" << entry.command() << ">";
	}
}

//检查每个raft对象index处的log是否处于一致，返回处于一致的raft对象个数
int Config::nCommitted(uint32_t index) {
	int count = 0;
	std::string cmd;
	{
		melon::MutexGuard lock(mutex_);
		for (uint32_t i = 0; i < n_; ++i) {
			if (logs_[i].size() > index) {
				const std::string& cmd1 = logs_[i][index].command();
				if (count > 0) {
					EXPECT_EQ(cmd, cmd1);
				}
				count++;
				cmd = cmd1;
			}
		}
	}
	return count;
}

//向leader发送一个命令，等待日志同步，检查是否所有raft对象达成一致
int Config::one(const std::string& cmd, int expected_server, bool retry) {
	melon::Timestamp t0 = melon::Timestamp::now();
	melon::Timestamp now;
	int starts = 0;
	do {
		uint32_t index = 0;
		for (uint32_t i = 0; i < n_; ++i) {
			starts = (starts + 1) % n_;
			if (raft_connected_[starts]) {
				uint32_t index1;
				uint32_t term;
				bool is_leader = rafts_[starts]->start(cmd, index1, term);
				if (is_leader) {
					index = index1;
					break;
				}
			}
		}
		if (index != 0) {
			// somebody claimed to be the leader and to have
			// submitted our command; wait a while for agreement.
			melon::Timestamp t1 = melon::Timestamp::now();
			do {
				int nc = nCommitted(index);
				if (nc > 0 && nc >= expected_server) {
					LOG_INFO << "pass one(), index=" << index;
					return index;
				}
				usleep(20 * 1000);
				now = melon::Timestamp::now();
			} while((now.getSec() - t1.getSec()) < 2);
			if (retry == false) {
				LOG_FATAL << cmd << " failed to reach agreement";
			}
			
		} else {
			usleep(50 * 1000);
		}
		
		now = melon::Timestamp::now();
	} while (now.getSec() - t0.getSec() < 10);
	LOG_FATAL << " failed to reach agreement";
	return -1;
}

}
