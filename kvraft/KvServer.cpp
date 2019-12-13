#include "KvServer.h"
#include "common.h"

#include <unistd.h>

namespace cherry {
	
KvServer::KvServer(const std::vector<PolishedRpcClient::Ptr>& peers, 
					uint32_t me, 
					melon::IpAddress addr,
					melon::Scheduler* scheduler)
				: me_(me),
				  scheduler_(scheduler),
				  raft(peers, me, addr, scheduler) {
	melon::rpc::RpcServer& server = raft.getRpcServer();
	raft.setApplyFunc(std::bind(&KvServer::applyFunc, this, std::placeholders::_1, std::placeholders::_2));
	server.registerRpcHandler<KvCommnad>(std::bind(&KvServer::onCommand, this, std::placeholders::_1));
}

MessagePtr KvServer::onCommand(std::shared_ptr<KvCommnad> args) {
	std::shared_ptr<KvCommnadReply> reply = std::make_shared<KvCommnadReply>();
	bool is_leader = raft.isLeader();
	if (!is_leader) {
		reply->set_leader(false);
	} else {
		reply->set_leader(true);
		auto it = latest_applied_seq_per_client_.find(args->cid());
		if (it != latest_applied_seq_per_client_.end() && args->seq() <= it->second) {
			if (args->operation() == operation::GET) {
				auto key_it = db_.find(args->key());
				if (key_it == db_.end()) {
					reply->set_error(operation::ERROR_NO_KEY);
				} else {
					reply->set_value(key_it->second);
					reply->set_error(operation::ERROR_OK);
				}
			}
		} else {
			std::string cmd;
			args->SerializeToString(&cmd);
			uint32_t term;
			uint32_t index;
			raft.start(cmd, index, term);

			// 等待，直到日志达成一致
			melon::CoroutineCondition cond;
			notify_[index] = cond;
			// TODO:处理超时的情况
			notify_[index].wait();

			if (args->operation() == operation::GET) {
				auto key_it = db_.find(args->key());
				if (key_it == db_.end()) {
					reply->set_error(operation::ERROR_NO_KEY);
					printf("GET: no key %s\n", args->key().c_str());
				} else {
					reply->set_value(key_it->second);
					reply->set_error(operation::ERROR_OK);
					printf("GET: key %s, value is %s\n", args->key().c_str(), reply->value().c_str());
				}
			}
		}

	}

	return reply;
}

void KvServer::applyFunc(uint32_t, LogEntry log) {
	KvCommnad cmd;
	cmd.ParseFromString(log.command());
	latest_applied_seq_per_client_[cmd.cid()] = cmd.seq();
	if (cmd.operation() == operation::GET) {
		//do nothing
	} else if (cmd.operation() == operation::PUT) {
		db_[cmd.key()] = cmd.value();
		printf("PUT: <%s, %s>\n", cmd.key().c_str(), cmd.value().c_str());
	} else if (cmd.operation() == operation::APPEND) {
		db_[cmd.key()] += cmd.value();
		printf("APPEND: <%s, %s>\n", cmd.key().c_str(), cmd.value().c_str());
	} else if (cmd.operation() == operation::DELETE) {
		db_.erase(cmd.key());
		printf("DELETE: key %s\n", cmd.key().c_str());
	} else {
		LOG_ERROR << "invalid command operation";
	}

	auto it = notify_.find(log.index());
	if (it != notify_.end()) {
		it->second.notify();
		notify_.erase(it);
	}
}

}

int main(int args, char* argv[]) {
	using namespace melon;
	Logger::setLogLevel(LogLevel::INFO);
	Singleton<Logger>::getInstance()->addAppender("console", LogAppender::ptr(new ConsoleAppender()));

	if (args < 5) {
		printf("Usage: %s n me base_port peer_ips\n", argv[0]);
		return 0;
	}
	int n = std::atoi(argv[1]);
	uint32_t me = std::atoi(argv[2]);
	int base_port = std::atoi(argv[3]);
	if (args < 4 + n) {
		printf("Usage: %s n me base_port peer_ips\n", argv[0]);
		return 0;
	}

	Scheduler scheduler;

	std::vector<cherry::PolishedRpcClient::Ptr> peers;
	for (int i = 0; i < n; ++i) {
		IpAddress peer_addr(argv[4 + i], base_port + i);
		peers.push_back(std::make_shared<cherry::PolishedRpcClient>(peer_addr, &scheduler));
	}

	IpAddress server_addr(base_port + me);
	cherry::KvServer kvserver(peers, me, server_addr, &scheduler);
	sleep(2);

	kvserver.start();
	printf("%d start\n", me);
	scheduler.start();
	return 0;
}
