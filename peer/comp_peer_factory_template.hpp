/*
 * comp_peer_factory_template.hpp
 *
 *  Created on: Nov 2, 2012
 *      Author: vjeko
 */

#ifndef COMP_PEER_FACTORY_TEMPLATE_HPP_
#define COMP_PEER_FACTORY_TEMPLATE_HPP_

#include <peer/comp_peer_factory.hpp>

template<size_t Num>
array<shared_ptr<comp_peer_t>, Num>
CompPeer_factory::generate(shared_ptr<InputPeer> input_peer) {

  array<shared_ptr<comp_peer_t>, Num> comp_peer_seq;

   int64_t id = 0;
  for (auto& cp : comp_peer_seq) {
    id++;
    cp = shared_ptr<comp_peer_t>(new comp_peer_t(id, input_peer));
  }

  for(shared_ptr<comp_peer_t>& cp: comp_peer_seq) {
    //cp->local_peers_ = comp_peer_seq;
  }

  for(shared_ptr<comp_peer_t>& cp: comp_peer_seq) {
    for(auto i = 0; i < Num; i++) {
      cp->net_peers_[i] = shared_ptr<RPCClient>(
          new RPCClient(cp->io_service_, "localhost", i + 1 + 10000));
    }
  }



  return comp_peer_seq;

}


#endif /* COMP_PEER_FACTORY_TEMPLATE_HPP_ */
