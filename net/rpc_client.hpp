#ifndef RPC_CLIENT_HPP_
#define RPC_CLIENT_HPP_

#include <common.hpp>

#include <net/session.hpp>
#include <bgp/vertex.hpp>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

class Peer;

class RPCClient {

  typedef boost::graph_traits<graph_t>::vertex_descriptor vertex_t;
  typedef boost::graph_traits<graph_t>::edge_descriptor edge_t;

public:

  RPCClient(
      io_service& io_service,
      string hostname,
      int64_t port);

  void publish(string key, int64_t value, vertex_t vertex);

  void read_impl(char* data, size_t length, tcp::socket& socket);

  void handle_write(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred);

  void handle_read(
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

  void new_write(char* data);
  void write_loop();

  void init(sync_init& si);
  void sync(vector<update_vertex_t>& nodes);

  boost::barrier* barrier_;
  boost::mutex m_;

  boost::mutex zzz_;
  boost::asio::deadline_timer timer_;

  std::queue<char*> buffer_queue_;

  io_service& io_service_;

  sync_response::hostname_mappings_t* hm_;
  uint32_t size_;
  update_vertex_t* array_;

  tcp::socket socket_;
  tcp::resolver resolver_;
  boost::asio::strand strand_;

  static log4cxx::LoggerPtr logger_;
};


#endif /* RPC_HPP_ */
