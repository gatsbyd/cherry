#include <melon/Log.h>
#include <melon/Address.h>
#include <vector>
#include "../src/Raft.h"

using namespace melon;

int main() {
	Singleton<Logger>::getInstance()->addAppender("console", LogAppender::ptr(new ConsoleAppender()));

	Scheduler scheduler;
	scheduler.startAsync();
	std::vector<rpc::RpcClient::Ptr> peers;
	cherry::Raft raft(peers, 0, IpAddress("127.0.0.1", 5000), &scheduler);
	return 0;
}
