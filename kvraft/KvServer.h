#ifndef _CHERRY_KVSERVER_H_
#define _CHERRY_KVSERVER_H_

#include "../src/Raft.h"
#include "../src/args.pb.h"

#include <melon/Coroutine.h>
#include <unordered_map>

namespace cherry {
	
class KvServer : public melon::Noncopyable {
public:
	KvServer(const std::vector<PolishedRpcClient::Ptr>& peers, 
					uint32_t me, 
					melon::IpAddress addr, 
					melon::Scheduler* scheduler);

	void start() { raft.start(); }

private:
	MessagePtr onCommand(std::shared_ptr<KvCommnad> args);
	void applyFunc(uint32_t server_id, LogEntry);

private:
	const std::string GET = "GET";
	const std::string PUT = "PUT";
	const std::string APPEND = "APPEND";
	const std::string DELETE = "DELETE";
	const std::string ERROR_NO_KEY = "ERROR_NO_KEY";
	const std::string ERROR_OK = "ERROR_OK";
	uint32_t me_;
	melon::Scheduler* scheduler_;
	Raft raft;
	
	std::unordered_map<std::string, std::string> db_;
	std::unordered_map<int64_t, uint32_t> latest_applied_seq_per_client_;
	std::unordered_map<uint32_t, melon::CoroutineCondition> notify_;
};


}
#endif
