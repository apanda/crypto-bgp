#ifndef COMP_PEER_HPP_
#define COMP_PEER_HPP_

#include <common.hpp>

#include <map>
#include <deque>
#include <atomic>

#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_queue.h>

#include <boost/array.hpp>
#include <boost/shared_ptr.hpp>

#include <gsl/gsl_blas.h>

#include <bgp/bgp.hpp>
#include <peer/peer.hpp>
#include <peer/input_peer.hpp>

class InputPeer;

template<const size_t Num>
class CompPeer : public Peer {
public:

  CompPeer(size_t id, shared_ptr<InputPeer> input_peer,
      std::unordered_map<int, shared_ptr<boost::barrier> > b);
  ~CompPeer();


  void evaluate(vector<string> circut, vertex_t l);
  void evaluate(string a, string b);

  symbol_t execute(vector<string> circut, vertex_t l);

  symbol_t add(string first, string second, string recombination_key, vertex_t l);
  symbol_t sub(string first, string second, string recombination_key, vertex_t l);
  symbol_t multiply(string first, string second, string recombination_key, vertex_t l);
  symbol_t multiply_const(string first, int64_t second, string recombination_key, vertex_t l);
  symbol_t recombine(string recombination_key, vertex_t key);

  symbol_t generate_random_num(string key, vertex_t l);
  symbol_t generate_random_bit(string key, vertex_t l);
  symbol_t generate_random_bitwise_num(string key, vertex_t l);

  int compare(string key1, string key2, vertex_t key);

  symbol_t continue_or_not(vector<string> circut,
      const string key,
      const int64_t result,
      string recombination_key,
      vertex_t l);

  void publish_all(symbol_t key,  int64_t value);

  boost::random::random_device rng_;

  size_t id_;

  typedef array<shared_ptr<CompPeer<Num> >, Num> localPeersImpl;
  typedef array<shared_ptr<RPCClient>, Num> netPeersImpl;

  netPeersImpl net_peers_;
  shared_ptr<InputPeer> input_peer_;
  shared_ptr<BGPProcess> bgp_;

  gsl_vector* recombination_vercor_;
  array<double, Num> recombination_array_;
  std::unordered_map<int, shared_ptr<boost::barrier> > barrier_map_;
};

typedef CompPeer<COMP_PEER_NUM> comp_peer_t;


#include <peer/comp_peer_template.hpp>

#endif /* COMPPEER_HPP_ */
