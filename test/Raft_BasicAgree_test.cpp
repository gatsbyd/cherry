#include "Config.h"
#include "gtest/gtest.h"

#include <melon/Log.h>

using namespace melon;

namespace cherry {

TEST(raft_agree_test, TestBasicAgree) {
	Logger::setLogLevel(LogLevel::INFO);
	Singleton<Logger>::getInstance()->addAppender("console", LogAppender::ptr(new ConsoleAppender()));

	uint32_t servers = 5;
	Scheduler scheduler;
	scheduler.startAsync();
	Config cfg(servers, &scheduler);
	cfg.start();

	int iters = 3;
	for (int index = 1; index < iters + 1; ++index) {
		int nd = cfg.nCommitted(index);
		if (nd > 0) {
			LOG_FATAL << "some have committed before Start()";
		}

		int xindex = cfg.one(std::to_string(index*100), servers, false);
		if (xindex != index) {
			LOG_FATAL << "got index " << xindex << " but expected " << index;
		}
	}
}

}
