#include <bgp/vertex.hpp>
#include <bgp/edge.hpp>

const vertex_t Vertex::UNDEFINED  = 9999999;

Vertex::Vertex() :
    mutex_(new mutex_t),
    id_(0), next_hop_(UNDEFINED) {

}


int Vertex::get_export(vertex_t to_vertex) {

  BOOST_ASSERT_MSG(next_hop_ != Vertex::UNDEFINED, "Impossible state!");

  BOOST_ASSERT( relationship_.find(next_hop_) != relationship_.end() );
  BOOST_ASSERT( relationship_.find(to_vertex) != relationship_.end() );

  auto next_rel = relationship_[next_hop_];
  auto from_rel = relationship_[to_vertex];

  if (next_rel == Vertex::REL::PROVIDER || next_rel == Vertex::REL::PEER) {
    if (from_rel == Vertex::REL::PROVIDER || from_rel == Vertex::REL::PEER) {
      return 0;
    }
  }

  return 1;
}


void Vertex::set_neighbors(graph_t& graph) {

  const auto range = adjacent_vertices(id_, graph);
  neigh_ = vector<vertex_t>(range.first, range.second);
  std::sort(neigh_.begin(), neigh_.end());
}



void Vertex::set_preference() {

  for(auto& neigh: neigh_) {
    preference_.at(neigh);
  }
  preference_.insert(std::make_pair(UNDEFINED, 0) );
}



int64_t Vertex::current_next_hop_preference(graph_t& graph) {

  if (next_hop_ == UNDEFINED) return 0;
  return preference_[graph[next_hop_].id_];
}



bool Vertex::in_as_path(graph_t& graph, vertex_t vertex) {

  vertex_t next = next_hop_;
  int MAX_AS_LENGTH = 10;
  int count = 0;

  while(true) {
    count++;

    if (next == vertex) return true;
    else if (next == UNDEFINED) return false;
    Vertex& v = graph[next];
    next = v.next_hop_;
    if (count > MAX_AS_LENGTH)  {
      return true;
    }
  }

  return false;
}


void Vertex::set_next_hop(graph_t& graph, vertex_t neigh) {
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
