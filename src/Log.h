#ifndef _CHERRY_LOG_H_
#define _CHERRY_LOG_H_

#include <stdint.h>
#include <memory>

namespace cherry {
	
class Command {
public:
	typedef std::shared_ptr<Command> Ptr;
	Command() = default;
	virtual ~Command() = default;
};

struct LogEntry {
	LogEntry(uint32_t term, uint32_t index, Command::Ptr command)
			:term_(term), index_(index), command_(command) {}
	uint32_t term_;
	uint32_t index_;
	Command::Ptr command_;
};


}
#endif
