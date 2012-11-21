#ifndef COMMON_HPP_
#define COMMON_HPP_

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

#include <iostream>
#include <vector>



using boost::adjacency_list;
using boost::graph_traits;

class Vertex;
class Edge;

typedef adjacency_list<
    boost::vecS,
    boost::vecS,
    boost::undirectedS,
    Vertex,
    Edge
    > graph_t;

typedef graph_traits<graph_t>::vertex_descriptor vertex_t;
typedef graph_traits<graph_t>::edge_descriptor edge_t;

#include "vertex.hpp"
#include "edge.hpp"

#endif /* COMMON_HPP_ */
