#ifndef COMP_PEER_FACTORY_HPP_
#define COMP_PEER_FACTORY_HPP_

#include "common.hpp"
#include "comp_peer.hpp"

class Comp_peer_factory {
public:

  Comp_peer_factory();

  template<size_t Num>
  array<shared_ptr<comp_peer_t>, Num>
  generate(shared_ptr<Input_peer> input_peer);

};

#include "comp_peer_factory_template.hpp"

#endif /* COMP_PEER_FACTORY_HPP_ */
