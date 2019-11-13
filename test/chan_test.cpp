#include "../src/Raft.h"
#include <stdio.h>
#include <memory>
#include <functional>
#include <melon/Thread.h>
#include <unistd.h>

int main() {
	melon::Scheduler scheduler;
	melon::Thread thread([&](){
						scheduler.start();
					});
	thread.start();

	chan_init_global();
	chan_t* append_chan = chan_init(0);
	chan_t* election_timer_chan = chan_init(0);

	scheduler.runAfter(3 * melon::Timestamp::kMicrosecondsPerSecond, 
								std::make_shared<melon::Coroutine>([=](){
									char message[] = "message";
									chan_send(election_timer_chan, message);						
								}));
	chan_t* chans[2] = {append_chan, election_timer_chan};
	void* msg;
	switch (chan_select(chans, 2, &msg, nullptr, 0, nullptr)) {
		case 0:
			printf("append rpc\n");
			break;
		case 1:
			printf("timeout\n");
			break;
	}


	chan_dispose(append_chan);
	chan_dispose(election_timer_chan);
	chan_dispose_global();
	return 0;
}
