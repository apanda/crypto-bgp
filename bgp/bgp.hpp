/*
 * bgp.hpp
 *
 *  Created on: Nov 27, 2012
 *      Author: vjeko
 */

#ifndef BGP_HPP_
#define BGP_HPP_

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

#include <iostream>
#include <vector>

#include "common.hpp"




void read_graphviz(std::string path, graph_t& graph);


void init(graph_t& graph);


void start(graph_t& graph);


void process_neighbors(
    const vertex_t affected_vertex,
    graph_t& graph,
    set<vertex_t>& changed_set,
    set<vertex_t>& new_changed_set);


void next_iteration(
    graph_t& graph,
    set<vertex_t>& affected_set,
    set<vertex_t>& changed_set);


void print_state(
    graph_t& graph,
    set<vertex_t>& affected_set,
    set<vertex_t>& changed_set);


void print_state(graph_t& graph);

#endif /* BGP_HPP_ */
