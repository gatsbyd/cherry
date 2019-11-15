#ifndef _CHERRY_CONFIG_H_
#define _CHERRY_CONFIG_H_

#include <vector>

namespace cherry {

class Raft;

class Config {
public:
	Config(int n);
	
private:
 int n_;
 std::vector<Raft*> rafts;

};

}
#endif
