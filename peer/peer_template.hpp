
#ifndef PEER_TEMPLATE_HPP_
#define PEER_TEMPLATE_HPP_

#include <string>

template<class PeerSeq>
void Peer::distribute_secret(
    const symbol_t key, const int value,
    PeerSeq comp_peers) {

  Secret<plaintext_t, COMP_PEER_NUM> secret(value);
  auto shares = secret.share();
  distribute_shares(key, shares, comp_peers);
}



template<class Values, class PeerSeq>
void Peer::distribute_shares(
    const symbol_t key, const Values values,
    PeerSeq comp_peers) {

  for(int i = 0; i < COMP_PEER_NUM; i++) {
    const auto share = values[i];
    comp_peers[i]->publish(key, share);
  }

}



#endif /* PEER_TEMPLATE_HPP_ */
