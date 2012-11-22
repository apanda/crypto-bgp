#include "vertex.hpp"

Vertex::Vertex() :
id_(0), next_hop_(UNDEFINED) {}

void Vertex::set_preference() {

  size_t preference = 0;
  for(auto& neigh: neigh_) {
    preference_.insert(std::make_pair(neigh, preference) );
    preference++;
  }
}



std::ostream& operator<<(std::ostream& os, const Vertex& vertex) {

  os << "-----------------------------------\n";
  os << "ID: " << vertex.id_ << std::endl;
  os << "Next Hop: " << vertex.next_hop_ << std::endl;
  for(auto pair: vertex.preference_) {
    os << "Neigh: " << pair.first << " ";
    os << "Preference: " << pair.second << std::endl;;
  }
  os << "-----------------------------------\n";

  return os;
}
