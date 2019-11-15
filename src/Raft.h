#ifndef _CHERRY_RAFT_H_
#define _CHERRY_RAFT_H_

#include "args.pb.h"
#include "channel/chan.h"
#include "PolishedRpcClient.h"

#include <melon/Address.h>
#include <melon/Mutex.h>
#include <melon/RpcServer.h>
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

	Raft(const std::vector<PolishedRpcClient::Ptr>& peers, uint32_t me, melon::IpAddress addr, melon::Scheduler* scheduler);
	~Raft();

	void start();
	bool start(MessagePtr cmd);
	void quit();

private:
	void resetLeaderState();
	void raftLoop();
	void turnToFollower(uint32_t);
	void turnToCandidate();
	void turnToLeader();
	void poll();
	void heartbeat();

	uint32_t getLastEntryIndex() const;
	const LogEntry& getLogEntryAt(uint32_t index) const;
	bool isMoreUpToDate(uint32_t last_log_index, uint32_t last_log_term) const;

	//rpc
	MessagePtr onRequestVote(std::shared_ptr<RequestVoteArgs> vote_args);
	MessagePtr onRequestAppendEntry(std::shared_ptr<RequestAppendArgs> append_args);
	bool sendRequestVote(uint32_t server, std::shared_ptr<RequestVoteArgs> vote_args);
	void onRequestVoteReply(std::shared_ptr<RequestVoteArgs> vote_args, std::shared_ptr<RequestVoteReply> vote_reply);
	bool sendRequestAppend(uint32_t server, std::shared_ptr<RequestAppendArgs> append_args);

private:
	State state_;
	uint32_t me_;
	//persistent state on all servers
	uint32_t current_term_;
	int32_t voted_for_;
	uint32_t voted_gain_;
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
	std::vector<PolishedRpcClient::Ptr> peers_;
	melon::Thread raft_loop_thread_;
	melon::Mutex mutex_;
	chan_t* election_timer_chan_;
	chan_t* heartbeat_timer_chan_;
	chan_t* append_chan_;
	chan_t* grant_to_candidate_chan_;
	chan_t* vote_result_chan_;
};
}
#endif
