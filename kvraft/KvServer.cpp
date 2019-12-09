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
	server.registerRpcHandler<KvCommnad>(std::bind(&KvServer::onCommand, this, std::placeholders::_1));

}

MessagePtr KvServer::onCommand(std::shared_ptr<KvCommnad> args) {
	std::shared_ptr<KvCommnadReply> reply = std::make_shared<KvCommnadReply>();
	bool is_leader = raft.isLeader();
	if (!is_leader) {
		reply->set_leader(false);
	} else {
		auto it = latest_reply_.find(args->cid());
		if (it != latest_reply_.end() && args->seq() <= it->second.seq) {
			reply->set_leader(true);
			reply->set_value(it->second.value);
			reply->set_error(it->second.error);
		} else {
			std::string cmd;
			args->SerializeToString(&cmd);
			uint32_t term;
			uint32_t index;
			raft.start(cmd, index, term);

			// 等待，直到日志达成一致
		}

	}

	return reply;
}


}
