#ifndef _POLISHED_RPC_CLIENT_H_
#define _POLISHED_RPC_CLIENT_H_

#include <melon/RpcClient.h>

namespace cherry {
template <typename T>
class TypeTraits {
    static_assert(std::is_base_of<::google::protobuf::Message, T>::value, "T must be subclass of google::protobuf::Message");
 public:
    typedef std::function<void (std::shared_ptr<T>)> ResponseHandler;
};


class PolishedRpcClient {
public:
	typedef std::shared_ptr<PolishedRpcClient> Ptr;
	PolishedRpcClient(const melon::IpAddress& server_addr, melon::Scheduler* scheduler):client(server_addr, scheduler){}
	void setConnected(bool connected) {
		connected_ = connected;
	}

	template <typename T>
	inline bool Call(melon::rpc::MessagePtr request, const typename TypeTraits<T>::ResponseHandler& handler) {
		if (connected_) {
			client.Call<T>(request, handler);
			return true;
		} else {
			return false;
		}
	}




private:
	melon::rpc::RpcClient client;
	bool connected_ = true;
};

}
#endif
