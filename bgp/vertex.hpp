#ifndef VERTEX_HPP_
#define VERTEX_HPP_

#include <common.hpp>
#include <bgp/common.hpp>

#include <tbb/concurrent_unordered_map.h>
#include <boost/thread/mutex.hpp>

#include <limits>
#include <queue>
#include <set>

using std::set;
using std::queue;

class RPCClient;
class RPCServer;

class Vertex {
public:

  friend std::ostream& operator<<(std::ostream& os, const Vertex& vertex);

  Vertex();
  void set_preference();
  void set_neighbors(graph_t& graph);
  void set_next_hop(graph_t& graph, vertex_t vertex);
  bool in_as_path(graph_t& graph, vertex_t vertex);
  int64_t current_next_hop_preference(graph_t graph);

  vertex_t id_;

  vertex_t next_hop_;
  vector<vertex_t> as_path_;
  set<vertex_t> as_path_set_;

  vector<vertex_t> neigh_;
  unordered_map<vertex_t, int64_t> preference_;

  typedef tbb::concurrent_unordered_map<string, int64_t> value_map_t;
  std::unordered_map<int, array<shared_ptr<RPCClient>, 3> > clients_;
  array<shared_ptr<RPCServer>, 3> servers_;

  value_map_t* values_;

  typedef boost::mutex mutex_t;
  typedef boost::lock_guard<mutex_t> lock_t;
  typedef boost::condition_variable condition_variable_t;

  tbb::concurrent_unordered_map<symbol_t, int64_t> value_map_;

  std::unordered_map<string, int> couter_map_2;
  tbb::concurrent_unordered_map<string, shared_ptr<mutex_t> > mutex_map_2;
  tbb::concurrent_unordered_map<string, shared_ptr<condition_variable_t> >cv_map_2;

  tbb::concurrent_unordered_map<string, shared_ptr<boost::function<void()> > > sig_map_x;
  tbb::concurrent_unordered_map<string, shared_ptr<boost::function<void(int)> > > sig_map_2x;
  tbb::concurrent_unordered_map<string, shared_ptr<boost::function<void()> > > sig_map_0x;
  tbb::concurrent_unordered_map<string, shared_ptr<boost::function<void()> > > sig_map_3x;

  static const vertex_t UNDEFINED;
};

#endif /* VERTEX_HPP_ */
