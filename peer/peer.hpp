#ifndef PEER_HPP_
#define PEER_HPP_

#include <common.hpp>
#include <net/rpc_client.hpp>
#include <net/rpc_server.hpp>
#include <net/session.hpp>
#include <bgp/edge.hpp>
#include <atomic>
#include <unordered_map>

#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/locks.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>

#include <tbb/compat/condition_variable>
#include <tbb/concurrent_unordered_map.h>

class BGPProcess;
class RPCServer;
class Session;

class Vertex;



typedef boost::graph_traits<graph_t>::vertex_descriptor vertex_t;
typedef boost::graph_traits<graph_t>::edge_descriptor edge_t;

class Peer {
public:
  Peer(io_service& io);
  virtual ~Peer();

  template<class PeerSeq>
  void distribute_secret(
      const symbol_t key, const int64_t value, const vertex_t vertex,
      PeerSeq comp_peers);

  template<class Values, class PeerSeq>
  static void distribute_shares(
      const symbol_t key, const Values values, const int64_t secret, const vertex_t vertex,
      PeerSeq comp_peers);

  virtual void subscribe(std::string key,  int64_t value, vertex_t vertex)  ;

  virtual void publish(std::string key,  int64_t value, vertex_t vertex);
  virtual void publish(Session* session, vector<vertex_t>& nodes, size_t id = 0);
  virtual void publish(Session* session, sync_init si);

  void print_values();

  typedef tbb::concurrent_unordered_map<int, int64_t> inter_map_t;
  typedef tbb::concurrent_unordered_map<symbol_t, int64_t> value_map_t;

  //typedef tbb::mutex mutex_t;
  //typedef tbb::interface5::unique_lock<tbb::mutex> lock_t;
  //typedef tbb::interface5::condition_variable condition_variable_t;

  typedef boost::mutex mutex_t;
  typedef boost::lock_guard<mutex_t> lock_t;
  typedef boost::condition_variable condition_variable_t;


  io_service& io_service_;
  int counter_;

  mutex_t barrier_mutex_;
  std::string recombination_key_;
  inter_map_t intermediary_;
  boost::thread_group tg_;
  static log4cxx::LoggerPtr logger_;
};



#include <peer/peer_template.hpp>

#endif /* PEER_HPP_ */
