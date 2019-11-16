#include "Raft.h"


#include <melon/Log.h>
#include <assert.h>
#include <functional>

namespace cherry {

Raft::Raft(const std::vector<PolishedRpcClient::Ptr>& peers, uint32_t me, melon::IpAddress addr, melon::Scheduler* scheduler)
	:state_(Follower),
	me_(me),
	current_term_(0),
	voted_for_(-1),
	voted_gain_(0),
	commit_index_(0),
	last_applied_(0),
	running_(false),
	scheduler_(scheduler),
	server_(addr, scheduler),
	peers_(peers),
	raft_loop_thread_(std::bind(&Raft::raftLoop, this), "raft loop") {
		server_.registerRpcHandler<RequestVoteArgs>(std::bind(&Raft::onRequestVote, this, std::placeholders::_1));
		server_.registerRpcHandler<RequestAppendArgs>(std::bind(&Raft::onRequestAppendEntry, this, std::placeholders::_1));
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

void Raft::start() {
	running_ = true;
	raft_loop_thread_.start();
}

bool Raft::start(MessagePtr cmd) {
	//todo
	(void) cmd;
	return true;
}

void Raft::quit() {
	running_ = false;
}

bool Raft::isLeader() {
	return state_ == Leader;
}

uint32_t Raft::term() {
	return current_term_;
}

void Raft::resetLeaderState() {
	next_index_.clear();
	match_index_.clear();
	next_index_.insert(next_index_.begin(), peers_.size(), log_.size());
	match_index_.insert(match_index_.begin(), peers_.size(), 0);
}

void Raft::raftLoop() {
	//TODO:
	int milli_election_timeout = 300 + rand() % 201;		//range from 300-500ms
	int milli_heartbeat_interval = 100;
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
					case 0:		//收到append rpc
						//TODO:取消定时
						break;
					case 1:		//收到投票成功
						//TODO:取消定时
						break;
					case 2:		//选举超时
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
					case 0:			//收到append rpc
						//TODO:取消定时
						break;
					case 1:			//给别人投票成功
						//TODO:取消定时
						break;
					case 2:			//收到投票结果
						//TODO:取消定时
						break;
					case 3:
						turnToCandidate();
						break;
				}
				break;
			}
			case Leader: {
				scheduler_->addTask(std::bind(&Raft::heartbeat, this), "raft heartbeat");

				scheduler_->runAfter(milli_heartbeat_interval * 1000, 
										std::make_shared<melon::Coroutine>([this](){
													char message[] = "timeout";
													chan_send(election_timer_chan_, message);
												}));
				chan_t* chans[3] = {append_chan_, grant_to_candidate_chan_, heartbeat_timer_chan_};
				void* msg;
				switch (chan_select(chans, 3, &msg, nullptr, 0, nullptr)) {
					case 0:		//收到append rpc
						//TODO:取消定时
						break;
					case 1:		//给别人投票成功
						//TODO:取消定时
						break;
					case 2:		//心跳间隔
						break;
				}
				break;
			}
		}
	}
}

void Raft::turnToFollower(uint32_t term) {
	if (term > current_term_) {
		state_ = Follower;
		current_term_ = term;
	}
}

void Raft::turnToCandidate() {
	{
		melon::MutexGuard lock(mutex_);
		state_ = Candidate;
		current_term_ += 1;
		voted_for_ = me_;
		voted_gain_ = 0;
	}
	LOG_INFO << me_ << " become candidate";
}

void Raft::turnToLeader() {
	if (state_ != Candidate) {
		return;
	}
	{
		melon::MutexGuard lock(mutex_);
		state_ = Leader;
	}
	resetLeaderState();
}

void Raft::poll() {
	if (state_ != Candidate) {
		return;
	}
	//construct RequestVoteArgs
	std::shared_ptr<RequestVoteArgs> vote_args = std::make_shared<RequestVoteArgs>();
	{
		melon::MutexGuard lock(mutex_);
		vote_args->set_term(current_term_);
		vote_args->set_candidate_id(me_);
		vote_args->set_last_log_term(getLogEntryAt(getLastEntryIndex()).term());
		vote_args->set_last_log_index(getLastEntryIndex());
	}
	for (size_t i = 0; i < peers_.size(); ++i) {
		if (i != me_) {
			sendRequestVote(i, vote_args);
		}
	}
	
}

void Raft::heartbeat() {
	//TODO:拷贝日志
	LOG_INFO << "heartbeat";
}

uint32_t Raft::getLastEntryIndex() const {
	return log_.size() - 1;
}

const LogEntry& Raft::getLogEntryAt(uint32_t index) const {
	assert(index < log_.size());
	return log_[index];
}

bool Raft::isMoreUpToDate(uint32_t last_log_index, uint32_t last_log_term) const {
	//TODO:
	(void) last_log_index;
	(void) last_log_term;
	return false;
}

MessagePtr Raft::onRequestVote(std::shared_ptr<RequestVoteArgs> vote_args) {
	std::shared_ptr<RequestVoteReply> vote_reply = std::make_shared<RequestVoteReply>();
	//第一种情况args.Term < rf.currentTerm:直接返回false
	if (vote_args->term() < current_term_) {
		vote_reply->set_term(current_term_);
		vote_reply->set_vote_granted(false);
	} else {
		//第二种情况args.Term > rf.currentTerm:变为follower并且重置voteFor为空
		if (vote_args->term() > current_term_) {
			turnToFollower(vote_args->term());
			voted_for_ = -1;
			//TODO:persist
		}

		vote_reply->set_term(current_term_);
		if (voted_for_ == -1 && !isMoreUpToDate(vote_args->last_log_index(), vote_args->last_log_term())) {
			voted_for_ = vote_args->candidate_id();		
			//TODO::persist
			//TODO:consume grant_to_candidate_chan_?
			vote_reply->set_vote_granted(true);
			char message[] = "vote to candidate";
			chan_send(grant_to_candidate_chan_, message);
		} else {
			vote_reply->set_vote_granted(false);
		}
	}

	return vote_reply;
}

MessagePtr Raft::onRequestAppendEntry(std::shared_ptr<RequestAppendArgs> append_args) {
	//todo
	(void)append_args;
	return nullptr;
}

bool Raft::sendRequestVote(uint32_t server, std::shared_ptr<RequestVoteArgs> vote_args) {
	assert(server < peers_.size());
	return peers_[server]->Call<RequestVoteReply>(vote_args, std::bind(&Raft::onRequestVoteReply, this, vote_args, std::placeholders::_1));
}

void Raft::onRequestVoteReply(std::shared_ptr<RequestVoteArgs> vote_args, std::shared_ptr<RequestVoteReply> vote_reply) {
	if (state_ != Candidate || vote_args->term() != current_term_) {
		return;
	}

	if (current_term_ < vote_reply->term()) {
		turnToFollower(vote_reply->term());
		voted_for_ = -1;
		//TODO:persist
		char message[] = "vote failed";
		chan_send(vote_result_chan_, message);
		return;
	}

	if (vote_reply->vote_granted()) {
		voted_gain_++;
		if (state_ == Candidate && voted_gain_ > (peers_.size() / 2)) {
			turnToLeader();
			//TODO:persist
			char message[] = "vote success";
			chan_send(vote_result_chan_, message);
		}
	}
}

bool Raft::sendRequestAppend(uint32_t server, std::shared_ptr<RequestAppendArgs> append_args) {
	//todo
	(void) server;
	(void) append_args;
	return true;
}


}
