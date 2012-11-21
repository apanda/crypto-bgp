#include "vertex.hpp"



Vertex::Vertex() :
id_(0), type_(0) {}

void Vertex::set_preference() {

  size_t preference = 0;
  for(auto& neigh: neigh_) {
    preference_.insert(std::make_pair(neigh, preference) );
    preference++;
  }
}
