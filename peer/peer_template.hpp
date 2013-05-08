
#ifndef PEER_TEMPLATE_HPP_
#define PEER_TEMPLATE_HPP_

#include <string>

template<class PeerSeq>
void Peer::distribute_secret(
    const symbol_t key, const int64_t value, const vertex_t vertex,
    PeerSeq comp_peers) {

  Secret<plaintext_t, COMP_PEER_NUM> secret(value);
  auto shares = secret.share();

  distribute_shares(key, shares, value, vertex, comp_peers);
}



template<class Values, class PeerSeq>
void Peer::distribute_shares(
    const symbol_t key, const Values values, const int64_t secret, const vertex_t vertex,
    PeerSeq comp_peers) {

  for(size_t i = 0; i < COMP_PEER_NUM; i++) {
    const auto share = values[i];
    LOG4CXX_INFO( logger_, "Share (" << secret << ") " << i + 1 << ": " << share);
    comp_peers[i]->publish(key, share, vertex);
  }

}



#endif /* PEER_TEMPLATE_HPP_ */
