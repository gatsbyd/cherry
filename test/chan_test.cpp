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

	chan_init_global();
	chan_t* timeout_chan_ = chan_init(0);
	chan_t* append_chan_ = chan_init(0);
	int64_t timer_id = scheduler.runAfter(1 * 1000 * 1000, std::make_shared<Coroutine>([=](){
								printf("chan_send timeout\n");
								chan_send(timeout_chan_, (char*)"timeout");						
							}));
	chan_t* chans[2] = {append_chan_, timeout_chan_};
	void *msg;
	switch (chan_select(chans, 2, &msg, nullptr, 0, nullptr)) {
		case 0:
			scheduler.cancel(timer_id);
			printf("%s\n", (char*)msg);
			break;
		case 1:
			printf("%s\n", (char*)msg);
	}

	getchar();
	chan_dispose(timeout_chan_);
	chan_dispose(append_chan_);
	chan_dispose_global();
	return 0;
}
