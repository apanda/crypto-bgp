#ifndef VERTEX_HPP_
#define VERTEX_HPP_

#include <common.hpp>
#include <bgp/common.hpp>

#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_queue.h>
#include <boost/thread/mutex.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/asio/buffer.hpp>

#include <limits>
#include <queue>
#include <set>

using std::set;
using std::queue;

class RPCClient;
class RPCServer;

class Vertex {
public:

  enum REL {
    CUSTOMER = 2,
    PEER = 1,
    PROVIDER = 0
  };

  friend std::ostream& operator<<(std::ostream& os, const Vertex& vertex);

  Vertex();
  void set_preference();
  void set_neighbors(graph_t& graph);
  void set_next_hop(graph_t& graph, vertex_t vertex);
  bool in_as_path(graph_t& graph, vertex_t vertex);
  int64_t current_next_hop_preference(graph_t& graph);

  int get_export(vertex_t to_vertex);

  typedef boost::mutex mutex_t;
  typedef boost::lock_guard<mutex_t> lock_t;
  typedef boost::condition_variable condition_variable_t;

  mutex_t* mutex_;

  vertex_t id_;
  vertex_t next_hop_;
  vertex_t new_next_hop_;

  vector<vertex_t> as_path_;
  set<vertex_t> as_path_set_;

  vector<vertex_t> neigh_;
  tbb::concurrent_unordered_map<vertex_t, int64_t> preference_;
  tbb::concurrent_unordered_map<vertex_t, int64_t> relationship_;
  unordered_map<int64_t, set<vertex_t> > preference_setup_;

  tbb::concurrent_unordered_map<pair<vertex_t, vertex_t>, int64_t> export_;

  unordered_map<int, array<shared_ptr<RPCClient>, 3> > clients_;
  array<shared_ptr<RPCServer>, 3> servers_;

  tbb::concurrent_unordered_map<symbol_t, int64_t> value_map_;

  tbb::concurrent_unordered_map<string, int> couter_map_2;
  tbb::concurrent_unordered_map<
    string,
    shared_ptr<mutex_t>,
    tbb::tbb_hash<string>,
    std::equal_to<string>
  > mutex_map_2;

  tbb::concurrent_unordered_map<string, shared_ptr<condition_variable_t> >cv_map_2;

  tbb::concurrent_unordered_map<string, shared_ptr<boost::function<void()> > > sig_recombine;
  tbb::concurrent_unordered_map<string, shared_ptr<boost::function<void()> > > sig_compare;

  tbb::concurrent_unordered_map<string, shared_ptr<boost::function<void(int64_t)> > > sig_bgp_cnt;
  tbb::concurrent_unordered_map<string, shared_ptr<boost::function<void()> > > sig_bgp_next;

  tbb::concurrent_queue< boost::asio::mutable_buffer > data_queue;

  static const vertex_t UNDEFINED;
};

#endif /* VERTEX_HPP_ */
