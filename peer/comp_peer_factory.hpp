#ifndef COMP_PEER_FACTORY_HPP_
#define COMP_PEER_FACTORY_HPP_

#include <common.hpp>
#include <peer/comp_peer.hpp>

class CompPeer_factory {
public:

  CompPeer_factory();

  template<size_t Num>
  array<shared_ptr<comp_peer_t>, Num>
  generate(shared_ptr<InputPeer> input_peer, io_service& io);

};

#include <peer/comp_peer_factory_template.hpp>

#endif /* COMP_PEER_FACTORY_HPP_ */
