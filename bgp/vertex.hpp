#ifndef VERTEX_HPP_
#define VERTEX_HPP_

#include "common.hpp"

#include <limits>
#include <queue>
#include <set>

using std::set;
using std::queue;

static vertex_t UNDEFINED  = 9999999;

class Vertex {
public:

  friend std::ostream& operator<<(std::ostream& os, const Vertex& vertex);

  Vertex();
  void set_preference();

  vertex_t id_;

  vertex_t next_hop_;
  queue<vertex_t> as_path_;
  set<vertex_t> as_path_set_;

  vector<vertex_t> neigh_;
  unordered_map<vertex_t, size_t> preference_;
};

#endif /* VERTEX_HPP_ */
