#ifndef VERTEX_HPP_
#define VERTEX_HPP_

#include <bgp/common.hpp>

#include <limits>
#include <queue>
#include <set>

using std::set;
using std::queue;

class Vertex {
public:

  friend std::ostream& operator<<(std::ostream& os, const Vertex& vertex);

  Vertex();
  void set_preference();
  void set_neighbors(graph_t& graph);
  void set_next_hop(graph_t& graph, vertex_t vertex);
  bool in_as_path(graph_t& graph, vertex_t vertex);
  size_t current_next_hop_preference(graph_t graph);

  vertex_t id_;

  vertex_t next_hop_;
  queue<vertex_t> as_path_;
  set<vertex_t> as_path_set_;

  vector<vertex_t> neigh_;
  unordered_map<vertex_t, size_t> preference_;

  static const vertex_t UNDEFINED;
};

#endif /* VERTEX_HPP_ */
