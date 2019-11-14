#include "Raft.h"


#include <melon/Log.h>
#include <assert.h>
#include <functional>

namespace cherry {

Raft::Raft(const std::vector<melon::rpc::RpcClient::Ptr>& peers, uint32_t me, melon::IpAddress addr, melon::Scheduler* scheduler)
	:state_(Follower),
	me_(me),
	current_term_(0),
	voted_for_(-1),
	commit_index_(0),
	last_applied_(0),
	running_(false),
	scheduler_(scheduler),
	server_(addr, scheduler),
	peers_(peers),
	raft_loop_thread_(std::bind(&Raft::raftLoop, this), "raft loop") {
		server_.registerRpcHandler<RequestVoteArgs>(std::bind(&Raft::onRequestVote, this, std::placeholders::_1));
		server_.registerRpcHandler<RequestAppendArgs>(std::bind(&Raft::onRequestAppendEntry, this, std::placeholders::_1));
		raft_loop_thread_.start();
		running_ = true;

		chan_init_global();
		append_chan_ = chan_init(0);
		election_timer_chan_ = chan_init(0);
		grant_to_candidate_chan_ = chan_init(0);
		vote_result_chan_ = chan_init(0);

		//todo:read log_, voted_for_, current_term_ from persistent
		
		resetLeaderState();
}

Raft::~Raft() {
	chan_dispose_global();
	chan_dispose(append_chan_);
	chan_dispose(election_timer_chan_);
	chan_dispose(grant_to_candidate_chan_);
	chan_dispose(vote_result_chan_);
	quit();
	raft_loop_thread_.join();
}

bool Raft::start(MessagePtr cmd) {
	//todo
	(void) cmd;
	return true;
}

void Raft::quit() {
	running_ = false;
}

void Raft::resetLeaderState() {
	next_index_.clear();
	match_index_.clear();
	next_index_.insert(next_index_.begin(), peers_.size(), log_.size());
	match_index_.insert(match_index_.begin(), peers_.size(), 0);
}

void Raft::raftLoop() {
	//TODO:
	int milli_election_timeout = 1000;
	int milli_heartbeat_interval = 200;
	while (running_) {
		State state;
		{
			melon::MutexGuard lock(mutex_);
			state = state_;
		}
		switch (state) {
			case Follower: {
				scheduler_->runAfter(milli_election_timeout * 1000, 
										std::make_shared<melon::Coroutine>([this](){
													char message[] = "timeout";
													chan_send(election_timer_chan_, message);
												}));
				chan_t* chans[3] = {append_chan_, grant_to_candidate_chan_, election_timer_chan_};
				void* msg;
				switch (chan_select(chans, 3, &msg, nullptr, 0, nullptr)) {
					case 0:
						//do nothing
						break;
					case 1:
						//do nothing
						break;
					case 2:
						turnToCandidate();
						break;
				}
				break;
			}
			case Candidate: {
				scheduler_->addTask(std::bind(&Raft::poll, this), "raft poll");
				scheduler_->runAfter(milli_election_timeout * 1000, 
										std::make_shared<melon::Coroutine>([this](){
													char message[] = "timeout";
													chan_send(election_timer_chan_, message);
												}));
				chan_t* chans[4] = {append_chan_, grant_to_candidate_chan_, vote_result_chan_, election_timer_chan_};
				void* msg;
				switch (chan_select(chans, 4, &msg, nullptr, 0, nullptr)) {
					case 0:
						//do nothing
						break;
					case 1:
						//do nothing
						break;
					case 2:
						//do nothing
						break;
					case 3:
						turnToCandidate();
						break;
				}
				break;
			}
			case Leader: {
				scheduler_->addTask(std::bind(&Raft::heartbeat, this), "raft poll");

				scheduler_->runAfter(milli_heartbeat_interval * 1000, 
										std::make_shared<melon::Coroutine>([this](){
													char message[] = "timeout";
													chan_send(election_timer_chan_, message);
												}));
				chan_t* chans[3] = {append_chan_, grant_to_candidate_chan_, heartbeat_timer_chan_};
				void* msg;
				switch (chan_select(chans, 3, &msg, nullptr, 0, nullptr)) {
					case 0:
						//do nothing
						break;
					case 1:
						//do nothing
						break;
					case 2:
						//do nothing
						break;
				}
				break;
			}
		}
	}
}

void Raft::turnToCandidate() {
	{
		melon::MutexGuard lock(mutex_);
		state_ = Candidate;
		current_term_ += 1;
		voted_for_ = me_;
	}
	LOG_INFO << me_ << "become candidate";
}

void Raft::poll() {
	//TODO:发起选举
	LOG_INFO << "start poll";
}

void Raft::heartbeat() {
	//TODO:拷贝日志
	LOG_INFO << "heartbeat";
}

MessagePtr Raft::onRequestVote(std::shared_ptr<RequestVoteArgs> vote_args) {
	(void)vote_args;
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
