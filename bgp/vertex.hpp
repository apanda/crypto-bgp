#ifndef VERTEX_HPP_
#define VERTEX_HPP_

#include "common.hpp"

#include <unordered_map>

class Vertex {
public:
  Vertex();
  void set_preference();

  int id_;
  int type_;

  std::vector<vertex_t> neigh_;
  std::unordered_map<vertex_t, size_t> preference_;
};

#endif /* VERTEX_HPP_ */
