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

typedef int plaintext_t;
typedef string symbol_t;
typedef map<symbol_t, plaintext_t> plaintext_map_t;

class Input_peer : public Peer {
public:
  Input_peer();

  template<size_t Num>
  void result();

  template<class PlaintextMap, class CompPeerSeq>
  static void distribute_secrets(
      const PlaintextMap& secret_map,
      CompPeerSeq& comp_peers);

  plaintext_map_t plaintext_map_;
};

#include "input_peer_template.hpp"

#endif /* INPUT_PEER_HPP_ */
