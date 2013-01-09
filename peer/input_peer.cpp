#include <peer/input_peer.hpp>

InputPeer::InputPeer(io_service& io) : Peer(io)  {}

int InputPeer::compute_lsb(int64_t value) {

  int result = mod(value, PRIME);
  result = result % 2;

  return result;
}
