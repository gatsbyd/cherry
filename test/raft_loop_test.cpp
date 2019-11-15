#include <melon/Log.h>
#include <melon/Address.h>
#include <vector>
#include <stdio.h>
#include "../src/Raft.h"

using namespace melon;

int main() {
	Logger::setLogLevel(LogLevel::INFO);
	Singleton<Logger>::getInstance()->addAppender("console", LogAppender::ptr(new ConsoleAppender()));

	Scheduler scheduler;
	scheduler.startAsync();
	std::vector<cherry::PolishedRpcClient::Ptr> peers;
	cherry::Raft raft(peers, 0, IpAddress("127.0.0.1", 5000), &scheduler);
	getchar();
	return 0;
}
