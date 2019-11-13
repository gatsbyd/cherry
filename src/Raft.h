#ifndef _CHERRY_RAFT_H_
#define _CHERRY_RAFT_H_

#include "args.pb.h"
#include "channel/chan.h"

#include <melon/Address.h>
#include <melon/Mutex.h>
#include <melon/RpcServer.h>
#include <melon/RpcClient.h>
#include <melon/Thread.h>
#include <google/protobuf/message.h>
#include <atomic>
#include <memory>
#include <stdint.h>
#include <vector>

namespace cherry {

typedef std::shared_ptr<::google::protobuf::Message> MessagePtr;

class Raft {
public:
	enum State {
		Follower = 0,
		Candidate,
		Leader,
	};

	Raft(const std::vector<melon::rpc::RpcClient::Ptr>& peers, melon::IpAddress addr, melon::Scheduler* scheduler);
	~Raft();

	bool start(MessagePtr cmd);
	void quit();

private:
	void resetLeaderState();
	void raftLoop();

	//rpc
	MessagePtr onRequestVote(std::shared_ptr<RequestVoteArgs> vote_args);
	MessagePtr onRequestAppendEntry(std::shared_ptr<RequestAppendArgs> append_args);
	void sendRequestVote(uint32_t server, std::shared_ptr<RequestVoteArgs> vote_args);
	void sendRequestAppend(uint32_t server, std::shared_ptr<RequestAppendArgs> append_args);

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

	std::atomic_bool running_;
	melon::Scheduler* scheduler_;
	melon::rpc::RpcServer server_;
	std::vector<melon::rpc::RpcClient::Ptr> peers_;
	melon::Thread raft_loop_thread_;
	melon::Mutex mutex_;
	chan_t* append_chan_;
	chan_t* election_timer_chan_;
	chan_t* grant_to_candidate_chan_;
	chan_t* vote_result_chan_;
};
}
#endif
