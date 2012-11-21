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

int main() {

  graph_t graph;
  dynamic_properties dp;

  dp.property("node_id", get(&Vertex::id_, graph));
  dp.property("type", get(&Vertex::type_, graph));
  dp.property("key", get(&Edge::key_, graph));

  std::ifstream file("../scripts/dot.dot");

  bool status = read_graphviz(file ,graph,dp,"node_id");

  for (auto vp = vertices(graph); vp.first != vp.second; ++vp.first) {
    const auto& vertex = (*vp.first);
    Vertex& property = graph[vertex];

    const auto range = adjacent_vertices(vertex, graph);
    property.neigh_ = std::vector<vertex_t>(range.first, range.second);
    property.set_preference();

    std::cout << "-----------------------------------\n";
    std::cout << "ID: \t\t" << property.id_ << std::endl;
    std::cout << "Type: \t\t" << property.type_ << std::endl;
    for(auto pair: property.preference_) {
      std::cout << "Neigh: " << pair.first << " ";
      std::cout << "Preference: " << pair.second << std::endl;;
    }
    std::cout << "-----------------------------------\n";
  }

  return 0;

}
