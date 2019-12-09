#ifndef _CHERRY_KVSERVER_H_
#define _CHERRY_KVSERVER_H_

#include "../src/Raft.h"
#include "../src/args.pb.h"

#include <unordered_map>

namespace cherry {
	
class KvServer {
public:
	KvServer(const std::vector<PolishedRpcClient::Ptr>& peers, 
					uint32_t me, 
					melon::IpAddress addr, 
					melon::Scheduler* scheduler);


private:
	uint32_t me_;
	melon::Scheduler* scheduler_;
	Raft raft;
	
	std::unordered_map<std::string, std::string> db_;
};


}
#endif
