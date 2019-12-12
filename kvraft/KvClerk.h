#ifndef _CHERRY_KVCLERK_H_
#define _CHERRY_KVCLERK_H_

#include "../src/args.pb.h"
#include "../src/PolishedRpcClient.h"

#include <melon/Noncopyable.h>
#include <melon/Scheduler.h>

namespace cherry {

class KvClerk : public melon::Noncopyable {
public:
	KvClerk(int64_t cid, const std::vector<PolishedRpcClient::Ptr>& peers);
	bool get(const std::string& key, std::string& value);
	void put(const std::string& key, const std::string& value);
	void append(const std::string& key, const std::string& value);
	void del(const std::string& key);
private:
	void onCommandReply(std::shared_ptr<KvCommnad> cmd,
						std::shared_ptr<KvCommnadReply> reply);

	int64_t cid_;
	uint32_t seq_;
	int latest_leader_id_;
	std::vector<PolishedRpcClient::Ptr> peers_;
};

}
#endif
