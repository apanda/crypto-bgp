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

#include <tbb/concurrent_unordered_set.h>

#include <iostream>
#include <vector>

template<size_t >
class CompPeer;
class RPCClient;

class BGPProcess {

public:

  BGPProcess();
  BGPProcess(
      string path,
      CompPeer<3>* comp_peer,
      io_service& io);

  static void load_graph(std::string path, graph_t& graph);
  void init(graph_t& graph);

  void start(graph_t& graph);
  void start_callback(function<bool()> f);

  void process_neighbors(
      const vertex_t affected_vertex,
      graph_t& graph,
      set<vertex_t>& changed_set,
      set<vertex_t>& new_changed_set);

  void process_neighbors_mpc(
      const vertex_t affected_vertex,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > changed_set_ptr,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
      shared_ptr<size_t> count_ptr);

  void for1(
      vertex_t affected_vertex,
      vertex_t neigh_vertex,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
      int cmp);

  void for0(
      const vertex_t affected_vertex,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
      shared_ptr< size_t > count_ptr,
      shared_ptr< vector<vertex_t> > n);

  void next_iteration(
      vertex_t dst,
      graph_t& graph,
      shared_ptr< set<vertex_t> > affected_ptr,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > changed_ptr);

  void print_state(
      graph_t& graph,
      set<vertex_t>& affected_set,
      set<vertex_t>& changed_set);

  void print_state(graph_t& graph);
  void print_result();

  graph_t graph_;
  CompPeer<3>* comp_peer_;
  shared_ptr<boost::barrier> bp_;
  shared_ptr<RPCClient> master_;
  io_service& io_service_;

  boost::mutex m_;
  boost::condition_variable cv_;

};

#endif /* BGP_HPP_ */
