#ifndef SESSION_HPP_
#define SESSION_HPP_

#include <common.hpp>

#include <peer/peer.hpp>

class Peer;

class Session {
public:

  Session(boost::asio::io_service& io_service, Peer* peer) :
    socket_(io_service),
    peer_(peer) {
  }

  void start();


  void handle_read(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred);


  tcp::socket& socket();

  tcp::socket socket_;
  enum { length_ = 32 };
  Peer* peer_;

};

#endif /* SESSION_HPP_ */
