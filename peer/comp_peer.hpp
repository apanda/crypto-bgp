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
class Comp_peer : public Peer {
public:

  Comp_peer(size_t id, shared_ptr<Input_peer> input_peer);

  void execute(vector<string> circut);

  void add(string first, string second);
  void multiply(string first, string second);

  void generate_random_num();
  void generate_random_bit();
  void continue_or_not(vector<string> circut,
      const string key,
      const int result);
  void recombine();
  void publish_all(symbol_t key, int value);


  size_t id_;

  typedef array<shared_ptr<Comp_peer<Num> >, Num> localPeersImpl;
  typedef array<shared_ptr<RPCClient>, Num> netPeersImpl;

  netPeersImpl net_peers_;

  shared_ptr<Input_peer> input_peer_;
  shared_ptr<const gsl_vector> recombination_vercor_;

  boost::random::random_device rng_;
};

typedef Comp_peer<COMP_PEER_NUM> comp_peer_t;

#include "comp_peer_template.hpp"

#endif /* COMPPEER_HPP_ */
