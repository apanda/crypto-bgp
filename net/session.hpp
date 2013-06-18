#ifndef SESSION_HPP_
#define SESSION_HPP_

#include <common.hpp>

#include <bgp/vertex.hpp>
#include <peer/peer.hpp>

class Peer;

class Session {
public:

  Session(boost::asio::io_service& io_service,
      boost::asio::strand& strand,
      Peer* peer) :
    socket_(io_service),
    strand_(io_service),
    peer_(peer) {
  }

  void start();

  void write_impl(char* data, size_t length, tcp::socket& socket);
  void read_impl(char* data, size_t total_length, size_t  offset);

  void handle_write(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred);

  void handle_preread(
      int* data,
      const boost::system::error_code& error,
      size_t bytes_transferred);

  void handle_read(
      char* data,
      size_t where,
      const boost::system::error_code& error,
      size_t bytes_transferred);

  void handle_msg(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred);

  void handle_init(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred);

  void handle_sync(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred);


  void sync_response(struct sync_response& sr);
  void notify(vector<vertex_t>& nodes);
  static pair<char*, size_t> contruct_notification(vector<update_vertex_t>& nodes);

  tcp::socket& socket();

  tcp::socket socket_;
  boost::asio::strand strand_;
  Peer* peer_;

};

#endif /* SESSION_HPP_ */
