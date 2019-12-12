#include "KvClerk.h"

namespace cherry {

KvClerk::KvClerk(int64_t cid, 
				const std::vector<PolishedRpcClient::Ptr>& peers) 
				: cid_(cid),
				  seq_(0),
				  latest_leader_id_(0),
				  peers_(peers) {

}
	
bool KvClerk::get(const std::string& key, std::string& value) {
	std::shared_ptr<KvCommnad> cmd = std::make_shared<KvCommnad>();
	cmd->set_operation("GET");
	cmd->set_cid(cid_);
	cmd->set_seq(seq_);
	cmd->set_key(key);

	while (true) {
		bool success = peers_[latest_leader_id_]->Call<KvCommnadReply>(cmd, 
											std::bind(&KvClerk::onCommandReply, this, cmd, std::placeholders::_1));
		
	}
}
	
void KvClerk::put(const std::string& key, const std::string& value) {

}
	
void KvClerk::append(const std::string& key, const std::string& value) {

}
	
void KvClerk::del(const std::string& key) {

}

}
