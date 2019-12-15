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
	heartbeat_interval_(100),
	apply_func_(std::bind(&Raft::defaultApplyFunc, this, std::placeholders::_1, std::placeholders::_2)) {
		server_.registerRpcHandler<RequestVoteArgs>(std::bind(&Raft::onRequestVote, this, std::placeholders::_1));
		server_.registerRpcHandler<RequestAppendArgs>(std::bind(&Raft::onRequestAppendEntry, this, std::placeholders::_1));
		server_.start();
		//TODO:read log_, voted_for_, current_term_ from persistent

		//Figure 2表明logs下标从1开始，所以添加一个空的Entry
		LogEntry entry;
		entry.set_term(0);
		entry.set_index(0);
		log_.push_back(entry);

		resetLeaderState();
}

Raft::~Raft() {
	LOG_DEBUG << "~Raft()";
}

void Raft::start() {
	turnToFollower(0);
}

bool Raft::start(const std::string& cmd, uint32_t& index, uint32_t& term) {
	bool is_leader = state_ == Leader;
	term = current_term_;
	if (is_leader) {
		index = getLastEntryIndex() + 1;
		LogEntry entry;
		entry.set_term(term);
		entry.set_index(index);
		entry.set_command(cmd);
		log_.push_back(entry);
		LOG_INFO << toString() << " :receive new command " << cmd;
	}
	return is_leader;
}

void Raft::quit() {
	running_ = false;
	LOG_DEBUG << toString() << " :quit";
}

void Raft::poll() {
	if (state_ != Candidate) {
		return;
	}
	//construct RequestVoteArgs
	std::shared_ptr<RequestVoteArgs> vote_args = std::make_shared<RequestVoteArgs>();
	vote_args->set_term(current_term_);
	vote_args->set_candidate_id(me_);
	vote_args->set_last_log_term(getLogEntryAt(getLastEntryIndex()).term());
	vote_args->set_last_log_index(getLastEntryIndex());

	for (size_t i = 0; i < peers_.size(); ++i) {
		if (i != me_) {
			sendRequestVote(i, vote_args);
		}
	}
	
}

bool Raft::sendRequestVote(uint32_t server, std::shared_ptr<RequestVoteArgs> vote_args) {
	assert(server < peers_.size());
	LOG_DEBUG << toString() << " :send vote rpc to " << server;
	return peers_[server]->Call<RequestVoteReply>(vote_args, 
					std::bind(&Raft::onRequestVoteReply, this, vote_args, std::placeholders::_1));
}

void Raft::onRequestVoteReply(std::shared_ptr<RequestVoteArgs> vote_args, std::shared_ptr<RequestVoteReply> vote_reply) {
	LOG_DEBUG << toString() << " :onRequestVoteReply";
	if (current_term_ < vote_reply->term()) {
		LOG_DEBUG << toString() << " failed to election, become follower";
		turnToFollower(vote_reply->term());
	} else if (vote_reply->vote_granted() && state_ == Candidate && vote_args->term() == current_term_) {
		voted_gain_++;
		if (voted_gain_ > (peers_.size() / 2)) {
			turnToLeader();
		}
	}
}

MessagePtr Raft::onRequestVote(std::shared_ptr<RequestVoteArgs> vote_args) {
	LOG_DEBUG << toString() << " onRequestVote";
	std::shared_ptr<RequestVoteReply> vote_reply = std::make_shared<RequestVoteReply>();
	//第一种情况args.Term < rf.currentTerm:直接返回false
	if (vote_args->term() < current_term_) {
		vote_reply->set_term(current_term_);
		vote_reply->set_vote_granted(false);
		LOG_DEBUG << toString() << " rejected " << vote_args->candidate_id() << "'s vote request, args.term=" << vote_args->term();
	} else {
		//第二种情况args.Term > rf.currentTerm:变为follower并且重置voteFor为空
		if (vote_args->term() > current_term_) {
			turnToFollower(vote_args->term());
		}

		vote_reply->set_term(current_term_);
		//选举限制，见论文5.4节
		if (voted_for_ == -1 && !thisIsMoreUpToDate(vote_args->last_log_index(), vote_args->last_log_term())) {
			voted_for_ = vote_args->candidate_id();		
			vote_reply->set_vote_granted(true);
			LOG_DEBUG << toString() << " vote for " << vote_args->candidate_id();
		} else {
			vote_reply->set_vote_granted(false);
			LOG_DEBUG << toString() << " rejected " << vote_args->candidate_id() << "'s vote request, vote_for=" << voted_for_;
		}
	}

	return vote_reply;
}

void Raft::heartbeat() {
	if (state_ != Leader) {
		return;
	}
	for (size_t i = 0; i < peers_.size(); ++i) {
		if (i != me_) {
			std::shared_ptr<RequestAppendArgs> append_args = std::make_shared<RequestAppendArgs>();

			append_args->set_term(current_term_);
			append_args->set_leader_id(me_);
			int next_index = next_index_[i];
			append_args->set_pre_log_index(next_index - 1);
			append_args->set_pre_log_term(log_[next_index - 1].term());
			append_args->set_leader_commit(commit_index_);
			constructLog(next_index, append_args);

			sendRequestAppend(i, append_args);
		}
	}
}

bool Raft::sendRequestAppend(uint32_t target_server, std::shared_ptr<RequestAppendArgs> append_args) {
	assert(target_server < peers_.size());
	LOG_DEBUG << toString() << " :send append rpc to " << target_server;
	return peers_[target_server]->Call<RequestAppendReply>(append_args, 
					std::bind(&Raft::onRequestAppendReply, this, target_server, append_args, std::placeholders::_1));
}

void Raft::onRequestAppendReply(uint32_t target_server, std::shared_ptr<RequestAppendArgs> append_args, std::shared_ptr<RequestAppendReply> append_reply) {
	if (state_ != Leader && current_term_ != append_args->term()) {
		return;
	}
	if (append_reply->success()) {
		//成功后更新leader的nextIndex和matchIndex，并且更新commitIndex
		match_index_[target_server] = append_args->pre_log_index() + append_args->entries_size();
		next_index_[target_server] = match_index_[target_server] + 1;
		LOG_DEBUG << toString() << " update next_index[" << target_server << "] to " << next_index_[target_server];
		updateCommitIndex();
	} else { //返回false有两种原因:1.出现term比当前server大的， 2.参数中的PreLogIndex对应的Term和目标server中index对应的Term不一致
		if (append_args->term() < append_reply->term()) {
			turnToFollower(append_reply->term());
		} else {
			if (append_reply->conflict_term() == 0) {
				next_index_[target_server] = append_reply->conflict_index();
			} else {
				// 目标server从append_reply->conflict_index()到append_args->pre_log_index()之间的log term都是append_reply->conflict_term()
				uint32_t pre_log_index = append_args->pre_log_index();
				while ((pre_log_index >= append_reply->conflict_index())
								&& (log_[pre_log_index].term() != append_reply->conflict_term())) {
					pre_log_index--;
				}
				next_index_[target_server] = pre_log_index + 1;
			} 
			uint32_t next_index = next_index_[target_server];
			//调整log参数,重新发送append rpc
			std::shared_ptr<RequestAppendArgs> new_args = std::make_shared<RequestAppendArgs>();
			new_args->set_term(current_term_);
			new_args->set_leader_id(me_);
			new_args->set_pre_log_index(next_index - 1);
			new_args->set_pre_log_term(log_[next_index - 1].term());
			new_args->set_leader_commit(commit_index_);
			constructLog(next_index, new_args);
			sendRequestAppend(target_server, new_args);
			LOG_DEBUG << toString() << ": resend append rpc to " << target_server << " change pre_log_index to " << next_index - 1;
		}
	}
}

MessagePtr Raft::onRequestAppendEntry(std::shared_ptr<RequestAppendArgs> append_args) {
	LOG_DEBUG << toString() << " :receive append rpc from " << append_args->leader_id() << ", pre_log_index=" 
			<< append_args->pre_log_index() << ", pre_log_term=" << append_args->pre_log_term();
	std::shared_ptr<RequestAppendReply> append_reply = std::make_shared<RequestAppendReply>();
	append_reply->set_term(current_term_);

	if (append_args->term() < current_term_) {
		append_reply->set_success(false);

		append_reply->set_conflict_index(0);
		append_reply->set_conflict_term(0);
	} else {
		turnToFollower(append_args->term());

		uint32_t pre_log_index = append_args->pre_log_index();
		if ((pre_log_index == 0)
			|| (pre_log_index <= getLastEntryIndex() && log_[pre_log_index].term() == append_args->pre_log_term())) {
			//细节:https://thesquareplanet.com/blog/raft-qa/最后一个问题
			append_reply->set_success(true);
			append_reply->set_conflict_index(0);
			append_reply->set_conflict_term(0);

			LOG_DEBUG << toString() << " got " << append_args->entries_size() << " entry from " << append_args->leader_id();

			if (append_args->entries_size() > 0) {
				log_.erase(log_.begin() + pre_log_index + 1, log_.end());

				std::vector<LogEntry> new_logs;
				for (int i = 0; i < append_args->entries_size(); ++i) {
					const LogEntry& entry = append_args->entries(i);
					new_logs.push_back(entry);
				}

				const LogEntry& last_entry = append_args->entries(append_args->entries_size() - 1);
				uint32_t index = pre_log_index + append_args->entries_size() + 1;
				if (index <= getLastEntryIndex() && log_[index].term() >= last_entry.term()) {
					for (uint32_t i = index; i < log_.size(); ++i) {
						new_logs.push_back(log_[i]);
					}
				}
				
				log_.insert(log_.end(), new_logs.begin(), new_logs.end());

			}

			if (append_args->leader_commit() > commit_index_) {
				commit_index_ = std::min(append_args->leader_commit(), getLastEntryIndex());
				applyLogs();
			}
		} else {
			append_reply->set_success(false);
			uint32_t pre_log_index = append_args->pre_log_index();
			if (pre_log_index > getLastEntryIndex()) {
				append_reply->set_conflict_index(log_.size());
				append_reply->set_conflict_term(0);
				LOG_DEBUG << toString() << " onRequestAppendEntry(), pre_log_index=" << pre_log_index << " > getLastEntryIndex=" << getLastEntryIndex();
			} else {
				append_reply->set_conflict_term(getLogEntryAt(pre_log_index).term());
				uint32_t i = pre_log_index;
				while (i > 0 && getLogEntryAt(i).term() == append_reply->conflict_term()) {
					i--;
				}
				append_reply->set_conflict_index(i + 1);
				LOG_DEBUG << toString() << ": conflict_index=" << append_reply->conflict_index() << ", conflict_term=" << append_reply->conflict_term();
			}
		}
	}

	return append_reply;
}

void Raft::turnToFollower(uint32_t term) {
	if (state_ == Leader) {
		scheduler_->cancel(hearbeat_id_);
		LOG_DEBUG << toString() << " :cancel hearbeat_id " << hearbeat_id_;
	} else {
		scheduler_->cancel(timeout_id_);
		LOG_DEBUG << toString() << " :cancel timeout_id " << timeout_id_;
	}
	state_ = Follower;
	LOG_DEBUG << toString() << " :become Follower";
	current_term_ = term;
	voted_for_ = -1;
	voted_gain_ = 0;
	timeout_id_ = scheduler_->runAfter(getElectionTimeout() * 1000, 
							std::make_shared<melon::Coroutine>(std::bind(&Raft::turnToCandidate, this)));
	LOG_DEBUG << toString() << " :timeout_id " << timeout_id_;
}

void Raft::turnToCandidate() {
	scheduler_->cancel(timeout_id_);
	LOG_DEBUG << toString() << " :cancel timeout_id " << timeout_id_;
	state_ = Candidate;
	current_term_ += 1;
	voted_for_ = me_;
	voted_gain_ = 1;

	LOG_DEBUG << toString() << " :become Candidate";
	poll();
	timeout_id_ = scheduler_->runAfter(getElectionTimeout() * 1000, 
										std::make_shared<melon::Coroutine>(std::bind(&Raft::turnToCandidate, this)));
	LOG_DEBUG << toString() << " :timeout_id " << timeout_id_;
}

void Raft::turnToLeader() {
	scheduler_->cancel(timeout_id_);
	LOG_DEBUG << toString() << " :cancel timeout_id " << timeout_id_;
	state_ = Leader;
	LOG_DEBUG << toString() << " :become Leader";
	resetLeaderState();
	heartbeat();
	hearbeat_id_ = scheduler_->runEvery(heartbeat_interval_ * 1000,
											std::make_shared<melon::Coroutine>(std::bind(&Raft::heartbeat, this)));
	LOG_DEBUG << toString() << " :hearbeat_id " << hearbeat_id_;
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

bool Raft::thisIsMoreUpToDate(uint32_t last_log_index, uint32_t last_log_term) const {
	uint32_t this_last_log_index = getLastEntryIndex();
	uint32_t this_last_log_term = log_[this_last_log_index].term();
	return (this_last_log_term > last_log_term 
			|| (this_last_log_term == last_log_term && this_last_log_index > last_log_index));
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
	os << "server(id=" << me_ << ", state=" << stateString() << ", term=" << current_term_ << ", log=[";
	for (const LogEntry& entry : log_) {
		os << "<index=" << entry.index() << ", term=" << entry.term() << ">";
	}
	os << "], peers(";
	for (const PolishedRpcClient::Ptr& peer : peers_) {
		os << (peer->isConnected() ? "1" : "0") << ",";
	}
	os << "))";
	return os.str();
}

int Raft::getElectionTimeout() {
	return 300 + rand() % 201;		//range from 300-500ms
}

void Raft::applyLogs() {
	while (last_applied_ < commit_index_) {
		last_applied_++;
		apply_func_(me_, getLogEntryAt(last_applied_));
	}
}

void Raft::setApplyFunc(ApplyFunc apply_func) {
	apply_func_ = std::move(apply_func);
}

void Raft::defaultApplyFunc(u_int32_t server_id, LogEntry entry) {
	(void) server_id;
	LOG_DEBUG << toString() << " :apply msg index=" << entry.index();
}

void Raft::updateCommitIndex() {
	std::vector<uint32_t> match_index(match_index_);
	match_index[me_] = getLastEntryIndex();

	std::sort(match_index.begin(), match_index.end());
	uint32_t N = match_index[match_index.size() / 2];
	//不能commit非当前term的entry, 具体见论文5.4
	if (state_ == Leader && N > commit_index_ && log_[N].term() == current_term_) {
		commit_index_ = N;
		applyLogs();
	}
}

}
