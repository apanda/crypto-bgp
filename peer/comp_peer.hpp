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
  void recombine(vector<string> circut);
  void publish_all(symbol_t key, int value);

  size_t id_;

  array<shared_ptr<Comp_peer<Num> >, Num> all_peers_;
  array<shared_ptr<RPCClient>, Num> all_peers__;
  shared_ptr<Input_peer> input_peer_;
  shared_ptr<const gsl_vector> recombination_vercor_;
};

typedef Comp_peer<COMP_PEER_NUM> comp_peer_t;

#include "comp_peer_template.hpp"

#endif /* COMPPEER_HPP_ */
