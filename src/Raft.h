#ifndef _CHERRY_RAFT_H_
#define _CHERRY_RAFT_H_

#include "Log.h"

#include <stdint.h>
#include <vector>

namespace cherry {

class Raft {
public:
	enum State {
		Follower = 0,
		Candidate,
		Leader,
	};
	Raft();

	bool start(Command::Ptr cmd);
	

private:
	State state_;
	//persistent state on all servers
	uint32_t current_term_;
	int32_t voted_for_;
	std::vector<LogEntry> log_;

	//volatile state on all servers
	uint32_t commit_index_;
	uint32_t last_applied_;

	//valotile state on leaders
	std::vector<uint32_t> next_index_;
	std::vector<uint32_t> match_index_;
};


}
#endif
