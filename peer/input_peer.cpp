#include <peer/input_peer.hpp>

InputPeer::InputPeer() : Peer(12122, io_)  {}

int InputPeer::compute_lsb(int64_t value) {

  int result = mod(value, PRIME);
  result = result % 2;

  return result;
}
