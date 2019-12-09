#ifndef _CHERRY_KVSERVER_H_
#define _CHERRY_KVSERVER_H_

#include "../src/Raft.h"
#include "../src/args.pb.h"

#include <unordered_map>

namespace cherry {
	
class KvServer {
public:
	struct LatestReply {
		uint32_t seq;
		std::string value;
		std::string error;
	};

	KvServer(const std::vector<PolishedRpcClient::Ptr>& peers, 
					uint32_t me, 
					melon::IpAddress addr, 
					melon::Scheduler* scheduler);

	MessagePtr onCommand(std::shared_ptr<KvCommnad> args);

private:
	uint32_t me_;
	melon::Scheduler* scheduler_;
	Raft raft;
	
	std::unordered_map<std::string, std::string> db_;
	std::unordered_map<int64_t, LatestReply> latest_reply_;
};


}
#endif
