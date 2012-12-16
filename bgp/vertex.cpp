#include <bgp/vertex.hpp>


const vertex_t Vertex::UNDEFINED  = 9999999;

Vertex::Vertex() :
id_(0), next_hop_(UNDEFINED),
values_(new value_map_t){}



void Vertex::set_neighbors(graph_t& graph) {

  const auto range = adjacent_vertices(id_, graph);
  neigh_ = vector<vertex_t>(range.first, range.second);

}



void Vertex::set_preference() {

  size_t preference = 1;
  for(auto& neigh: neigh_) {
    preference_.insert(std::make_pair(neigh, preference) );
    preference++;
  }
  preference_.insert(std::make_pair(UNDEFINED, 0) );
}



int64_t Vertex::current_next_hop_preference(graph_t graph) {

  //boost::mutex::scoped_lock lock(m_);

  if (next_hop_ == UNDEFINED) return 0;
  return preference_[graph[next_hop_].id_];
}



bool Vertex::in_as_path(graph_t& graph, vertex_t vertex) {

  //boost::mutex::scoped_lock lock(m_);

  vertex_t next = next_hop_;
  int MAX_AS_LENGTH = 10;
  int count = 0;

  while(true) {
    count++;

    //printf("Loop: %lu!\n", next );
    if (next == vertex) return true;
    else if (next == UNDEFINED) return false;
    Vertex& v = graph[next];
    next = v.next_hop_;
    if (count > MAX_AS_LENGTH)  {
      //printf("Undefined behavior!\n" );
      return true;
    }
  }

  return false;
}


void Vertex::set_next_hop(graph_t& graph, vertex_t neigh) {

  //boost::mutex::scoped_lock lock(m_);

  next_hop_ = neigh;
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
