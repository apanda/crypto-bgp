#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

#include <iostream>

#include "vertex.hpp"
#include "edge.hpp"

using boost::adjacency_list;
using boost::graph_traits;

using boost::add_vertex;
using boost::add_edge;

using boost::property;
using boost::dynamic_properties;
using boost::vertex_name_t;

using boost::get;

int main() {

 typedef adjacency_list<
      boost::vecS,
      boost::vecS,
      boost::undirectedS,
      Vertex,
      Edge
      > graph_t;

  typedef graph_traits<graph_t>::vertex_descriptor vertex_t;
  typedef graph_traits<graph_t>::edge_descriptor edge_t;

  graph_t graph;
  dynamic_properties dp;

  dp.property("node_id", get(&Vertex::id_, graph));
  dp.property("type", get(&Vertex::type_, graph));
  dp.property("key", get(&Edge::key_, graph));

  std::istringstream
    gvgraph("graph G { 0 [node_id=0, type=99]; 1 [node_id=1, type=99] }");

  std::ifstream file("../scripts/dot.dot");

  bool status = read_graphviz(file ,graph,dp,"node_id");

  for (auto vp = vertices(graph); vp.first != vp.second; ++vp.first) {
    const auto& vertex = (*vp.first);
    const Vertex& property = graph[vertex];

    std::cout << "ID: " << property.id_ << std::endl;
    std::cout << "Type: " << property.type_ << std::endl;
  }

  return 0;

}
