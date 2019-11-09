#include "Raft.h"

#include <assert.h>
#include <functional>

namespace cherry {

Raft::Raft(const std::vector<melon::rpc::RpcClient::Ptr>& peers, melon::IpAddress addr, melon::Scheduler* scheduler)
	:state_(Follower),
	current_term_(0),
	voted_for_(-1),
	commit_index_(0),
	last_applied_(0),
	running_(false),
	server_(addr, scheduler),
	peers_(peers),
	raft_loop_thread_(std::bind(&Raft::raftLoop, this), "raft loop") {
		server_.registerRpcHandler<RequestVoteArgs>(std::bind(&Raft::onRequestVote, this, std::placeholders::_1));
		server_.registerRpcHandler<RequestAppendArgs>(std::bind(&Raft::onRequestAppendEntry, this, std::placeholders::_1));
		raft_loop_thread_.start();
		running_ = true;
}

Raft::~Raft() {

}

bool Raft::start(MessagePtr cmd) {
	//todo
	(void) cmd;
	return true;
}

void Raft::quit() {
	running_ = false;
}

void Raft::raftLoop() {
	while (running_) {
		

	}
}

MessagePtr Raft::onRequestVote(std::shared_ptr<RequestVoteArgs> vote_args) {
	(void) vote_args;
	return nullptr;
}

MessagePtr Raft::onRequestAppendEntry(std::shared_ptr<RequestAppendArgs> append_args) {
	//todo
	(void)append_args;
	return nullptr;
}

void Raft::sendRequestVote(uint32_t server, std::shared_ptr<RequestVoteArgs> vote_args) {
	assert(server < peers_.size());
	peers_[server]->Call<RequestVoteReply>(vote_args, [](std::shared_ptr<RequestVoteReply> vote_reply) {
						(void)vote_reply;
					});
}

void Raft::sendRequestAppend(uint32_t server, std::shared_ptr<RequestAppendArgs> append_args) {
	//todo
	(void) server;
	(void) append_args;
}


}
