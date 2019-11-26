#include "Raft.h"

#include <melon/Log.h>
#include <assert.h>
#include <sstream>
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
	heartbeat_interval_(100) {
		server_.registerRpcHandler<RequestVoteArgs>(std::bind(&Raft::onRequestVote, this, std::placeholders::_1));
		server_.registerRpcHandler<RequestAppendArgs>(std::bind(&Raft::onRequestAppendEntry, this, std::placeholders::_1));
		server_.start();
		//todo:read log_, voted_for_, current_term_ from persistent
		
		resetLeaderState();

		//Figure 2表明logs下标从1开始，所以添加一个空的Entry
		LogEntry entry;
		entry.set_term(0);
		entry.set_index(0);
		log_.push_back(entry);
}

Raft::~Raft() {
	LOG_INFO << "~Raft()";
}

void Raft::start() {
	turnToFollower(0);
}

bool Raft::start(MessagePtr cmd) {
	//todo
	(void) cmd;
	return true;
}

void Raft::quit() {
	running_ = false;
	LOG_INFO << toString() << " :quit";
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
		LOG_INFO << toString() << " :send vote rpc";
	}
	for (size_t i = 0; i < peers_.size(); ++i) {
		if (i != me_) {
			sendRequestVote(i, vote_args);
		}
	}
	
}

bool Raft::sendRequestVote(uint32_t server, std::shared_ptr<RequestVoteArgs> vote_args) {
	assert(server < peers_.size());
	return peers_[server]->Call<RequestVoteReply>(vote_args, 
					std::bind(&Raft::onRequestVoteReply, this, vote_args, std::placeholders::_1));
}

void Raft::onRequestVoteReply(std::shared_ptr<RequestVoteArgs> vote_args, std::shared_ptr<RequestVoteReply> vote_reply) {
	LOG_INFO << toString() << " :onRequestVoteReply";
	if (current_term_ < vote_reply->term()) {
		LOG_INFO << toString() << " failed to election, become follower";
		turnToFollower(vote_reply->term());
	} else if (vote_reply->vote_granted() && state_ == Candidate && vote_args->term() == current_term_) {
		voted_gain_++;
		if (voted_gain_ > (peers_.size() / 2)) {
			turnToLeader();
		}
	}
}

MessagePtr Raft::onRequestVote(std::shared_ptr<RequestVoteArgs> vote_args) {
	LOG_INFO << toString() << " onRequestVote";
	std::shared_ptr<RequestVoteReply> vote_reply = std::make_shared<RequestVoteReply>();
	//第一种情况args.Term < rf.currentTerm:直接返回false
	if (vote_args->term() < current_term_) {
		vote_reply->set_term(current_term_);
		vote_reply->set_vote_granted(false);
		LOG_INFO << toString() << " rejected " << vote_args->candidate_id() << "'s vote request, args.term=" << vote_args->term();
	} else {
		//第二种情况args.Term > rf.currentTerm:变为follower并且重置voteFor为空
		if (vote_args->term() > current_term_) {
			turnToFollower(vote_args->term());
		}

		vote_reply->set_term(current_term_);
		if (voted_for_ == -1 && !isMoreUpToDate(vote_args->last_log_index(), vote_args->last_log_term())) {
			voted_for_ = vote_args->candidate_id();		
			vote_reply->set_vote_granted(true);
			LOG_INFO << toString() << " vote for " << vote_args->candidate_id();
		} else {
			vote_reply->set_vote_granted(false);
			LOG_INFO << toString() << " rejected " << vote_args->candidate_id() << "'s vote request, vote_for=" << voted_for_;
		}
	}

	return vote_reply;
}

void Raft::heartbeat() {
	if (state_ != Leader) {
		return;
	}
	for (size_t i = 0; i < peers_.size(); ++i) {
		std::shared_ptr<RequestAppendArgs> append_args = std::make_shared<RequestAppendArgs>();
		{
			melon::MutexGuard lock(mutex_);
			append_args->set_term(current_term_);
			append_args->set_leader_id(me_);
			int next_index = next_index_[i];
			append_args->set_pre_log_index(next_index - 1);
			append_args->set_pre_log_term(log_[next_index - 1].term());
			append_args->set_leader_commit(commit_index_);
			
			constructLog(next_index, append_args);
		}
		if (i != me_) {
			sendRequestAppend(i, append_args);
		}
	}
	LOG_INFO << toString() << " :send append rpc";
}

bool Raft::sendRequestAppend(uint32_t server, std::shared_ptr<RequestAppendArgs> append_args) {
	assert(server < peers_.size());
	return peers_[server]->Call<RequestAppendReply>(append_args, 
					std::bind(&Raft::onRequestAppendReply, this, append_args, std::placeholders::_1));
}

void Raft::onRequestAppendReply(std::shared_ptr<RequestAppendArgs> append_args, std::shared_ptr<RequestAppendReply> append_reply) {
	(void) append_args;
	if (append_reply->success()) {
		//TODO:成功后更新leader的nextIndex和matchIndex，并且更新commitIndex
	} else { //返回false有两种原因:1.出现term比当前server大的， 2.参数中的PreLogIndex对应的Term和目标server中index对应的Term不一致
		if (current_term_ < append_reply->term()) {
			turnToFollower(append_reply->term());
		}
		//TODO:第二种情况
	}
}

MessagePtr Raft::onRequestAppendEntry(std::shared_ptr<RequestAppendArgs> append_args) {
	LOG_INFO << toString() << " :recevie append rpc from " << append_args->leader_id();
	std::shared_ptr<RequestAppendReply> append_reply = std::make_shared<RequestAppendReply>();
	if (append_args->term() < current_term_) {
		append_reply->set_term(current_term_);
		append_reply->set_success(false);
	} else {
		turnToFollower(append_args->term());
		//TODO:
		append_reply->set_term(current_term_);
		append_reply->set_success(true);
	}

	return append_reply;
}

void Raft::turnToFollower(uint32_t term) {
	if (state_ == Leader) {
		scheduler_->cancel(hearbeat_id_);
		LOG_INFO << toString() << " :cancel hearbeat_id " << hearbeat_id_;
	} else {
		scheduler_->cancel(timeout_id_);
		LOG_INFO << toString() << " :cancel timeout_id " << timeout_id_;
	}
	state_ = Follower;
	LOG_INFO << toString() << " :become Follower";
	current_term_ = term;
	voted_for_ = -1;
	voted_gain_ = 0;
	timeout_id_ = scheduler_->runAfter(getElectionTimeout() * 1000, 
							std::make_shared<melon::Coroutine>(std::bind(&Raft::turnToCandidate, this)));
	LOG_INFO << toString() << " :timeout_id " << timeout_id_;
}

void Raft::turnToCandidate() {
	{
		melon::MutexGuard lock(mutex_);
		state_ = Candidate;
		current_term_ += 1;
		voted_for_ = me_;
		voted_gain_ = 1;
	}
	LOG_INFO << toString() << " :become Candidate";
	poll();
	timeout_id_ = scheduler_->runAfter(getElectionTimeout() * 1000, 
										std::make_shared<melon::Coroutine>(std::bind(&Raft::turnToCandidate, this)));
	LOG_INFO << toString() << " :timeout_id " << timeout_id_;
}

void Raft::turnToLeader() {
	scheduler_->cancel(timeout_id_);
	LOG_INFO << toString() << " :cancel timeout_id " << timeout_id_;
	state_ = Leader;
	LOG_INFO << toString() << " :become Leader";
	resetLeaderState();
	heartbeat();
	hearbeat_id_ = scheduler_->runEvery(heartbeat_interval_ * 1000,
											std::make_shared<melon::Coroutine>(std::bind(&Raft::heartbeat, this)));
	LOG_INFO << toString() << " :hearbeat_id " << hearbeat_id_;
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

void Raft::constructLog(size_t next_index, std::shared_ptr<RequestAppendArgs> append_args) {
	for (size_t i = next_index; i < log_.size(); ++i) {
		const LogEntry& originEntry = log_[i];
		LogEntry* entry = append_args->add_entries();
		entry->set_term(originEntry.term());
		entry->set_index(originEntry.index());
		entry->set_command(originEntry.command());
	}
}

std::string Raft::stateString() {
	if (state_ == Follower) {
		return "Follower";
	} else if (state_ == Candidate) {
		return "Candidate";
	} else {
		return "Leader";
	}
}

std::string Raft::toString() {
	std::ostringstream os;
	os << "server(id=" << me_ << ", state=" << stateString() << ", term=" << current_term_ << ")";
	return os.str();
}

int Raft::getElectionTimeout() {
	return 300 + rand() % 201;		//range from 300-500ms
}

}
