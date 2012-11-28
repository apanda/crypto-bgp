#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

#include <iostream>
#include <vector>

#include "common.hpp"

using boost::adjacency_list;
using boost::graph_traits;

using boost::add_vertex;
using boost::add_edge;

using boost::property;
using boost::dynamic_properties;
using boost::vertex_name_t;

using boost::adjacent_vertices;

using boost::get;

void read_graphviz(std::string path, graph_t& graph) {

  dynamic_properties dp;
  std::ifstream file(path);

  dp.property("node_id", get(&Vertex::id_, graph));
  dp.property("key", get(&Edge::key_, graph));

  bool status = read_graphviz(file ,graph, dp,"node_id");
}



void init(graph_t& graph) {

  for (auto vp = vertices(graph); vp.first != vp.second; ++vp.first) {
    const auto& vertex = (*vp.first);
    Vertex& property = graph[vertex];

    const auto range = adjacent_vertices(vertex, graph);
    property.neigh_ = vector<vertex_t>(range.first, range.second);
    property.set_preference();
  }

}




void print_state(
    graph_t& graph,
    set<vertex_t>& affected_set,
    set<vertex_t>& changed_set) {

  std::cout << "Changed: ";
  for(auto a: affected_set) {
    std::cout << a << " ";
  }
  std::cout << std::endl;;

  std::cout << "Affected: ";
  for(auto a: changed_set) {
    std::cout << a << " ";
  }
  std::cout << std::endl;;

}



void next_iteration(
    graph_t& graph,
    set<vertex_t>& affected_set,
    set<vertex_t>& changed_set) {

  std::cout << "Next itteration..." << std::endl;

  set<vertex_t> new_affected_set;
  set<vertex_t> new_changed_set;

  for(const vertex_t affected_vertex: affected_set) {
    std::cout << "Current vertex: " << affected_vertex << std::endl;

    if(affected_vertex == 0) continue;
    Vertex& affected = graph[affected_vertex];

    auto neighbors = adjacent_vertices(affected_vertex, graph);

    for(; neighbors.first != neighbors.second; ++neighbors.first) {
      const vertex_t& neigh_vertex = *(neighbors.first);
      std::cout << "\tNeighbor: " << neigh_vertex << std::endl;

      if(changed_set.find(neigh_vertex) != changed_set.end()) {
        Vertex& neigh = graph[neigh_vertex];

        const auto current_preference = affected.current_next_hop_preference(graph);
        const auto offered_preference = affected.preference_[neigh_vertex];
        std::cout << "\tCurrent preference: " << current_preference << std::endl;
        std::cout << "\tOffered preference: " << offered_preference << std::endl;

        if ( offered_preference <= current_preference ) continue;
        if ( neigh.in_as_path(graph, affected_vertex) ) continue;

        affected.set_next_hop(graph, neigh_vertex);

        neighbors = adjacent_vertices(affected_vertex, graph);
        new_changed_set.insert(affected_vertex);
        new_affected_set.insert(neighbors.first, neighbors.second);
      }

    }

  }


  if( !new_changed_set.empty() ) {
    print_state(graph, new_affected_set, new_changed_set);
    next_iteration(graph, new_affected_set, new_changed_set);
  }

}



void start(graph_t& graph) {

  Vertex& dst = graph[0];

  dst.next_hop_ = 0;
  dst.as_path_.push(dst.id_);
  dst.as_path_set_.insert(dst.id_);

  set<vertex_t> affected;
  set<vertex_t> changed;

  changed.insert(0);
  for(const vertex_t& vertex: dst.neigh_) {
    affected.insert(vertex);
  }

  next_iteration(graph, affected, changed);

}



int main() {

  graph_t graph;

  read_graphviz("../scripts/dot.dot", graph);
  init(graph);

  start(graph);

  for (auto vp = vertices(graph); vp.first != vp.second; ++vp.first) {
    const auto& vertex = (*vp.first);
    Vertex& property = graph[vertex];
    std::cout << property;
  }

  return 0;

}
