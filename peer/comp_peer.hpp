#ifndef COMP_PEER_HPP_
#define COMP_PEER_HPP_

#include <map>
#include <deque>
#include <atomic>

#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_queue.h>

#include <boost/array.hpp>
#include <boost/shared_ptr.hpp>

#include <gsl/gsl_blas.h>

#include <peer/peer.hpp>
#include <peer/input_peer.hpp>
#include <common.hpp>



template<const size_t Num>
class CompPeer : public Peer {
public:

  CompPeer(size_t id, shared_ptr<InputPeer> input_peer);

  void execute(vector<string> circut);

  void add(string first, string second);

  void multiply(string first, string second, string recombination_key);
  void recombine(string recombination_key);

  void generate_random_num(string key);
  void generate_random_bit(string key);
  void generate_random_bitwise_num(string key);

  void prefix_or();

  void continue_or_not(vector<string> circut,
      const string key,
      const  int64_t result);

  void publish_all(symbol_t key,  int64_t value);

  boost::random::random_device rng_;

  size_t id_;

  typedef array<shared_ptr<CompPeer<Num> >, Num> localPeersImpl;
  typedef array<shared_ptr<RPCClient>, Num> netPeersImpl;

  netPeersImpl net_peers_;
  shared_ptr<InputPeer> input_peer_;

  shared_ptr<const gsl_vector> recombination_vercor_;
  array<double, Num> recombination_array_;
};

typedef CompPeer<COMP_PEER_NUM> comp_peer_t;

#include "comp_peer_template.hpp"

#endif /* COMPPEER_HPP_ */
