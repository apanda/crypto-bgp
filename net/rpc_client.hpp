#ifndef RPC_CLIENT_HPP_
#define RPC_CLIENT_HPP_

#include <common.hpp>

#include <net/session.hpp>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

class Peer;

class RPCClient {

  class Vertex;
  class Edge;

  typedef boost::adjacency_list<
      boost::vecS,
      boost::vecS,
      boost::undirectedS,
      Vertex,
      Edge
      > graph_t;

  typedef boost::graph_traits<graph_t>::vertex_descriptor vertex_t;
  typedef boost::graph_traits<graph_t>::edge_descriptor edge_t;

public:

  RPCClient(io_service& io_service, string hostname,  int64_t port, Peer* peer = NULL);

  void publish(string key, int64_t value, vertex_t vertex);

  void handle_read(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred);

  void handle_write(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred);


  tcp::socket socket_;
  tcp::resolver resolver_;
  boost::asio::strand strand_;
  enum { length_ = 256 + 8 + 8 };

  static log4cxx::LoggerPtr logger_;
};


#endif /* RPC_HPP_ */
