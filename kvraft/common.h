#ifndef _CHERRY_KVRAFT_COMMON_H_
#define _CHERRY_KVRAFT_COMMON_H_

#include <string>

namespace cherry {
namespace operation {

const std::string GET = "GET";
const std::string PUT = "PUT";
const std::string APPEND = "APPEND";
const std::string DELETE = "DELETE";

const std::string ERROR_NO_KEY = "ERROR_NO_KEY";
const std::string ERROR_OK = "ERROR_OK";
}
}
#endif
