/*
 * bgp.hpp
 *
 *  Created on: Nov 27, 2012
 *      Author: vjeko
 */

#ifndef BGP_HPP_
#define BGP_HPP_

#include <common.hpp>
#include <bgp/common.hpp>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

#include <iostream>
#include <vector>

//#include <peer/comp_peer.hpp>

template<size_t >
class CompPeer;

class BGPProcess {

public:

  BGPProcess();
  BGPProcess(string path, CompPeer<3>* comp_peer);

  void load_graph(std::string path, graph_t& graph);
  void init(graph_t& graph);
  void start(graph_t& graph);


  void startX(function<bool()> f) {

    //print_state(graph);
    start(graph_);

    f();

  }

  void print_result() {

    auto iter = vertices(graph_);
    auto last = iter.second;
    auto current = iter.first;

    printf("digraph G {\n");

    for (; current != last; ++current) {
      const auto& current_vertex = *current;
      Vertex& vertex = graph_[current_vertex];
      printf("%ld -> %ld;\n", vertex.id_, vertex.next_hop_);
    }

    printf("}\n");
  }

  void process_neighbors(
      const vertex_t affected_vertex,
      graph_t& graph,
      set<vertex_t>& changed_set,
      set<vertex_t>& new_changed_set);

  void process_neighbors_mpc(
      const vertex_t affected_vertex,
      graph_t& graph,
      set<vertex_t>& changed_set,
      set<vertex_t>& new_changed_set);

  void next_iteration(
      vertex_t dst,
      graph_t& graph,
      set<vertex_t>& affected_set,
      set<vertex_t>& changed_set);

  void print_state(
      graph_t& graph,
      set<vertex_t>& affected_set,
      set<vertex_t>& changed_set);


  void print_state(graph_t& graph);

  graph_t graph_;
  CompPeer<3>* comp_peer_;
};

#endif /* BGP_HPP_ */
