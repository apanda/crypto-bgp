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



void next_iteration(
    graph_t& graph,
    set<vertex_t>& affected,
    set<vertex_t>& changed) {

  std::cout << "Next itteration..." << std::endl;

  set<vertex_t> new_affected;
  set<vertex_t> new_changed;

  for(const vertex_t current: affected) {
    //std::cout << "Current vertex: " << current << std::endl;

    if(current == 0) continue;
    Vertex& a = graph[current];

    auto neighbors = adjacent_vertices(current, graph);

    for(; neighbors.first != neighbors.second; ++neighbors.first) {
      const vertex_t& neigh = *(neighbors.first);
      //std::cout << "\tNeighbor: " << neigh << std::endl;

      if(changed.find(neigh) != changed.end()) {
        Vertex& v = graph[neigh];

        if(a.next_hop_ != UNDEFINED) {

          if(a.preference_[neigh] > a.preference_[graph[a.next_hop_].id_]) {
            a.next_hop_ = neigh;

            a.as_path_ = v.as_path_;
            a.as_path_.push(neigh);

            a.as_path_set_ = v.as_path_set_;
            a.as_path_set_.insert(neigh);

            neighbors = adjacent_vertices(current, graph);
            new_changed.insert(current);
            new_affected.insert(neighbors.first, neighbors.second);
          }

        } else {

          if(v.as_path_set_.find(current) == v  .as_path_set_.end()) {
            a.next_hop_ = neigh;

            a.as_path_ = v.as_path_;
            a.as_path_.push(neigh);

            a.as_path_set_ = v.as_path_set_;
            a.as_path_set_.insert(neigh);

            neighbors = adjacent_vertices(current, graph);
            new_changed.insert(current);
            new_affected.insert(neighbors.first, neighbors.second);
          }

        }

      }

    }

  }


  if( !new_changed.empty() ) {


    std::cout << "Changed: ";
    for(auto a: new_changed) {
      std::cout << a << " ";
    }
    std::cout << std::endl;;

    std::cout << "Affected: ";
    for(auto a: new_affected) {
      std::cout << a << " ";
    }
    std::cout << std::endl;;

    next_iteration(graph, new_affected, new_changed);
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
