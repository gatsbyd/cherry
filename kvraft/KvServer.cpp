#include "KvServer.h"

namespace cherry {
	
KvServer::KvServer(const std::vector<PolishedRpcClient::Ptr>& peers, 
					uint32_t me, 
					melon::IpAddress addr,
					melon::Scheduler* scheduler)
				: me_(me),
				  scheduler_(scheduler),
				  raft(peers, me, addr, scheduler) {
	melon::rpc::RpcServe& server = raft.getServer();
	server.registerRpcHandler<KvCommnad>();

}


}
