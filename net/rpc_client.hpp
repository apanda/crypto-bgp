#ifndef RPC_CLIENT_HPP_
#define RPC_CLIENT_HPP_

#include <common.hpp>
#include <net/session.hpp>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

class RPCClient {

public:

  RPCClient(io_service& io_service, string hostname,  int64_t port);

  void publish(string key, int64_t value);

  void handle_write(
      const boost::system::error_code& error,
      size_t bytes_transferred);


  tcp::socket socket_;
  tcp::resolver resolver_;
  enum { length_ = 32 };

  static log4cxx::LoggerPtr logger_;
};


#endif /* RPC_HPP_ */
