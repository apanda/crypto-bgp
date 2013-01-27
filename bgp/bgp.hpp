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
#include <boost/graph/adjacency_matrix.hpp>
#include <boost/graph/graphviz.hpp>

#include <tbb/concurrent_unordered_set.h>
#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_vector.h>

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
  static void load_graph2(std::string path, graph_t& graph);
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
      shared_ptr< pair<size_t, size_t> > counts_ptr);

  void compute_partial0(
      const vertex_t affected_vertex,
      shared_ptr< vertex_t > largest_vertex,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
      shared_ptr< tbb::concurrent_vector<vertex_t> > local_set_ptr,
      shared_ptr< pair<size_t, size_t> > counts_ptr,
      shared_ptr< pair<size_t, size_t> > subcounter_ptr,
      shared_ptr< vector<vertex_t> > intersection_ptr,
      pair<vector<vertex_t>::iterator, vector<vertex_t>::iterator> iters);

  void for0(
      const vertex_t affected_vertex,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
      shared_ptr< pair<size_t, size_t> > counts_ptr,
      shared_ptr< vector<vertex_t> > intersection_ptr);

  void compute_partial1(
      vertex_t affected_vertex,
      vertex_t neigh_vertex,
      shared_ptr< vertex_t > largest_vertex,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
      shared_ptr< tbb::concurrent_vector<vertex_t> > local_set_ptr,
      int cmp);

  void for1(
      vertex_t affected_vertex,
      vertex_t neigh_vertex,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
      int cmp);

  void next_iteration_start(
      vertex_t dst,
      shared_ptr< set<vertex_t> > affected_ptr,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > changed_ptr);

  void next_iteration_continue(
      const vertex_t dst_vertex,
      shared_ptr<vector<vertex_t> > batch_ptr,
      shared_ptr< set<vertex_t> > affected_set_ptr,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > changed_set_ptr,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr);

  void next_iteration_finish(
      vertex_t dst,
      shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_ptr);

  void print_state(
      graph_t& graph,
      set<vertex_t>& affected_set,
      set<vertex_t>& changed_set);

  void print_result();

  graph_t graph_;
  CompPeer<3>* comp_peer_;
  shared_ptr<boost::barrier> bp_;
  shared_ptr<RPCClient> master_;
  io_service& io_service_;

  boost::mutex m_;
  boost::condition_variable cv_;

  tbb::concurrent_queue< function<void()> > execution_stack_;

  function<void()> continuation_;
  function<void()> end_;

};

#endif /* BGP_HPP_ */
