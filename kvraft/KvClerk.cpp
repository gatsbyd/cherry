#include "KvClerk.h"
#include "common.h"

#include <sstream>

namespace cherry {

KvClerk::KvClerk(int64_t cid, 
				const std::vector<PolishedRpcClient::Ptr>& peers) 
				: cid_(cid),
				  seq_(0),
				  latest_leader_id_(0),
				  peers_(peers) {

}
	
bool KvClerk::get(const std::string& key, std::string& value) {
	std::shared_ptr<KvCommnadReply> reply = sendCommand(operation::GET, key);
	if (reply->error() == operation::ERROR_OK) {
		value = reply->value();
		return true;
	}
	return false;
}
	
void KvClerk::put(const std::string& key, const std::string& value) {
	sendCommand(operation::PUT, key, value);
	printf("PUT: key %s, value %s\n", key.c_str(), value.c_str());
}
	
void KvClerk::append(const std::string& key, const std::string& value) {
	sendCommand(operation::APPEND, key, value);
	printf("APPEND: key %s, value %s\n", key.c_str(), value.c_str());
}
	
void KvClerk::del(const std::string& key) {
	sendCommand(operation::DELETE, key);
	printf("DELETE: key %s\n", key.c_str());
}

std::shared_ptr<KvCommnadReply> KvClerk::sendCommand(const std::string& operation, 
											const std::string& key, 
											const std::string& value) {
	std::shared_ptr<KvCommnad> cmd = std::make_shared<KvCommnad>();
	cmd->set_operation(operation);
	cmd->set_cid(cid_);
	cmd->set_seq(seq_);
	cmd->set_key(key);
	if (operation == operation::PUT || operation == operation::APPEND) {
		cmd->set_value(value);
	}

	while (true) {
		std::shared_ptr<KvCommnadReply> reply = nullptr;

		melon::Mutex mutex;
		melon::Condition cond(mutex);
		peers_[latest_leader_id_]->Call<KvCommnadReply>(cmd, 
											std::bind([&reply, &cond](std::shared_ptr<KvCommnadReply> response){
														reply = response;
														cond.notify();
														//printf("notify\n");
													}, std::placeholders::_1));
		melon::MutexGuard lock(mutex);
		bool is_timeout = cond.wait_seconds(1);
		if (is_timeout) {
			//printf("timeout\n");
		} else {
			if (reply->leader()) {
				//printf("%d is leader\n", latest_leader_id_);
			} else {
				//printf("%d is not leader\n", latest_leader_id_);
			}
		}
		if (is_timeout || !reply || !reply->leader()) { 
			++latest_leader_id_;
			latest_leader_id_ %= peers_.size();
			//printf("retry server %d\n", latest_leader_id_);
			continue;
		} else {
			seq_++;
			//printf("return reply\n");
			return reply;
		}
	}
}

}

void usage(std::string operation) {
	if (operation == "get") {
		printf("Usage:get key\n");
	} else if (operation == "put") {
		printf("Usage:put key value\n");
	} else if (operation == "append") {
		printf("Usage:append key value\n");
	} else if (operation == "delete") {
		printf("Usage:delete key\n");
	} else {
		printf("unknown operation\n");
	}
}

int main(int argc, char* argv[]) {
	using namespace melon;
	using namespace cherry;
	if (argc < 3) {
		printf("Usage: %s n base_port server_ips\n", argv[0]);
		return 0;
	}
	int n = std::atoi(argv[1]);
	int base_port = std::atoi(argv[2]);
	if (argc < 3 + n) {
		printf("Usage: %s n base_port server_ips\n", argv[0]);
		return 0;
	}
	Scheduler scheduler;
	scheduler.startAsync();
	std::vector<cherry::PolishedRpcClient::Ptr> peers;
	for (int i = 0; i < n; ++i) {
		IpAddress peer_addr(argv[3 + i], base_port + i);
		peers.push_back(std::make_shared<cherry::PolishedRpcClient>(peer_addr, &scheduler));
	}

	//TODO:保证每个clerk的cid唯一
	KvClerk clerk(std::rand(), peers);
	std::string line, word;
	std::vector<std::string> words;
	while (getline(std::cin, line)) {
		words.clear();
		std::istringstream cmd(line);
		while (cmd >> word) {
			words.push_back(word);
		}
		if (words.size() == 0) {
			continue;
		}
		if (words[0] == "get") {
			if (words.size() < 2) {
				usage(words[0]);
				continue;
			}
			std::string value;
			bool exist = clerk.get(words[1], value);
			if (exist) {
				printf("value:%s\n", value.c_str());
			} else {
				printf("no such key\n");
			}
		} else if (words[0] == "put") {
			if (words.size() < 3) {
				usage(words[0]);
				continue;
			}
			clerk.put(words[1], words[2]);
		} else if (words[0] == "append") {
			if (words.size() < 3) {
				usage(words[0]);
				continue;
			}
			clerk.append(words[1], words[2]);
		} else if (words[0] == "delete") {
			if (words.size() < 2) {
				usage(words[0]);
				continue;
			}
			clerk.del(words[1]);
		} else if (words[0] == "quit") {
			printf("bye\n");
			break;
		} else {
			usage(words[0]);
			continue;
		}			
	}

	return 0;
}
