/*
 * input_peer.hpp
 *
 *  Created on: Oct 27, 2012
 *      Author: vjeko
 */

#ifndef INPUT_PEER_HPP_
#define INPUT_PEER_HPP_

#include <common.hpp>
#include <peer/peer.hpp>


using std::map;

typedef int64_t plaintext_t;
typedef string symbol_t;
typedef map<symbol_t, plaintext_t> plaintext_map_t;

class InputPeer : public Peer {
public:
  InputPeer();

  template<size_t Num>
  void result();

  template<class CompPeerSeq>
  static void lsb(
      string key,
      int64_t value,
      CompPeerSeq& comp_peers);

  template<class CompPeerSeq>
  static void bitwise_share(
      string key,
      int64_t value,
      CompPeerSeq& comp_peers);

  template<class CompPeerSeq>
  static void distribute_secret(
      symbol_t key, plaintext_t value,
      CompPeerSeq& comp_peers);

  template<class PlaintextMap, class CompPeerSeq>
  static void distribute_secrets(
      const PlaintextMap& secret_map,
      CompPeerSeq& comp_peers);

  template<class PlaintextMap, class CompPeerSeq>
  static void distribute_lsb(
      const PlaintextMap& secret_map,
      CompPeerSeq& comp_peers);

  plaintext_map_t plaintext_map_;
};

#include "input_peer_template.hpp"

#endif /* INPUT_PEER_HPP_ */
