#ifndef BGP_COMMON_HPP_
#define BGP_COMMON_HPP_

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/adjacency_matrix.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/timer/timer.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <unordered_map>

using boost::adjacency_list;
using boost::graph_traits;
using boost::timer::cpu_timer;

using std::string;
using std::vector;
using std::unordered_map;
using std::ostream;
using std::set;

using boost::adjacency_list;
using boost::graph_traits;
using boost::add_vertex;
using boost::add_edge;

using boost::property;
using boost::dynamic_properties;
using boost::vertex_name_t;

using boost::adjacent_vertices;

using boost::get;

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

#endif /* COMMON_HPP_ */
