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
	bool isLeader();
	uint32_t term();

private:
	void resetLeaderState();
	void raftLoop();
	void turnToFollower();
	void turnToCandidate();
	void turnToLeader();
	void poll();
	void heartbeat();

	uint32_t getLastEntryIndex() const;
	const LogEntry& getLogEntryAt(uint32_t index) const;
	bool isMoreUpToDate(uint32_t last_log_index, uint32_t last_log_term) const;
	void constructLog(size_t next_index, std::shared_ptr<RequestAppendArgs> append_args);
	std::string stateString();
	std::string toString();

	void consumeAndSet(chan_t* chan, char* messge);
	//vote rpc
	bool sendRequestVote(uint32_t server, std::shared_ptr<RequestVoteArgs> vote_args);
	void onRequestVoteReply(std::shared_ptr<RequestVoteArgs> vote_args, std::shared_ptr<RequestVoteReply> vote_reply);
	MessagePtr onRequestVote(std::shared_ptr<RequestVoteArgs> vote_args);

	//append rpc
	bool sendRequestAppend(uint32_t server, std::shared_ptr<RequestAppendArgs> append_args);
	void onRequestAppendReply(std::shared_ptr<RequestAppendArgs> append_args, std::shared_ptr<RequestAppendReply> append_reply);
	MessagePtr onRequestAppendEntry(std::shared_ptr<RequestAppendArgs> append_args);

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
	chan_t* append_chan_;			//收到心跳rpc后
	chan_t* grant_to_candidate_chan_;	//给别的Candidate投了一票
	chan_t* vote_result_chan_;		//某次选举有结果
};
}
#endif
