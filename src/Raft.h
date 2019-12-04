#ifndef _CHERRY_RAFT_H_
#define _CHERRY_RAFT_H_

#include "args.pb.h"
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

	typedef std::function<void(uint32_t, LogEntry)> ApplyFunc;

	Raft(const std::vector<PolishedRpcClient::Ptr>& peers, uint32_t me, melon::IpAddress addr, melon::Scheduler* scheduler);
	~Raft();

	void start();
	bool start(const std::string& cmd, uint32_t& index, uint32_t& term);
	void quit();
	bool isLeader();
	uint32_t term();
	void setApplyFunc(ApplyFunc apply_func);

	// for test
	const std::vector<PolishedRpcClient::Ptr>& getPeers() const { return peers_; }

private:
	void resetLeaderState();
	void turnToFollower(uint32_t term);
	void turnToCandidate();
	void turnToLeader();
	void poll();
	void heartbeat();

	uint32_t getLastEntryIndex() const;
	const LogEntry& getLogEntryAt(uint32_t index) const;
	bool thisIsMoreUpToDate(uint32_t last_log_index, uint32_t last_log_term) const;
	void constructLog(size_t next_index, std::shared_ptr<RequestAppendArgs> append_args);
	std::string stateString();
	std::string toString();
	int getElectionTimeout();
	void applyLogs();
	void defaultApplyFunc(uint32_t server_id, LogEntry);
	void updateCommitIndex();

	//vote rpc
	bool sendRequestVote(uint32_t server, std::shared_ptr<RequestVoteArgs> vote_args);
	void onRequestVoteReply(std::shared_ptr<RequestVoteArgs> vote_args, std::shared_ptr<RequestVoteReply> vote_reply);
	MessagePtr onRequestVote(std::shared_ptr<RequestVoteArgs> vote_args);

	//append rpc
	bool sendRequestAppend(uint32_t server, std::shared_ptr<RequestAppendArgs> append_args);
	void onRequestAppendReply(uint32_t target_server, std::shared_ptr<RequestAppendArgs> append_args, std::shared_ptr<RequestAppendReply> append_reply);
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
	int heartbeat_interval_;
	int64_t timeout_id_;
	int64_t hearbeat_id_;
	
	ApplyFunc apply_func_;
};
}
#endif
