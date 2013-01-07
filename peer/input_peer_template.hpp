#ifndef INPUT_PEER_TEMPLATE_HPP_
#define INPUT_PEER_TEMPLATE_HPP_

#include <common.hpp>
#include <bgp/bgp.hpp>
#include <bgp/vertex.hpp>

#include <bitset>

#include <gsl/gsl_interp.h>
#include <gsl/gsl_poly.h>

#include <boost/dynamic_bitset.hpp>


typedef Secret<plaintext_t, COMP_PEER_NUM> secret_t;


template<size_t Num>
void InputPeer::result() {}



template<class CompPeerSeq>
void InputPeer::distribute_secret(
    symbol_t key, plaintext_t value,
    CompPeerSeq& comp_peers) {

  secret_t secret(value);
  auto shares = secret.share();
  distribute_shares(key, shares, value, 0, comp_peers);

}



template<class PlaintextMap, class CompPeerSeq>
void InputPeer::distribute_lsb(
    const PlaintextMap& secret_map,
    CompPeerSeq& comp_peers) {

  vector<symbol_t> tmp;
  vector<int64_t> tmpv;

  for(auto pair : secret_map) {

    auto key = pair.first;
    tmp.push_back(key);

    auto value = pair.second;
    tmpv.push_back(value);
    lsb(key, value, comp_peers);
  }

  auto first = tmp.back();
  tmp.pop_back();

  auto second = tmp.back();
  tmp.pop_back();

  auto first_v = tmpv.back();
  tmpv.pop_back();

  auto second_v = tmpv.back();
  tmpv.pop_back();

  string key = second + "-" + first;
  int64_t value = second_v - first_v;
  lsb(key, value, comp_peers);

  key = first + "-" + second;
  value = first_v - second_v;
  lsb(key, value, comp_peers);

}



template<class PlaintextMap, class CompPeerSeq>
void InputPeer::distribute_secrets(
    const PlaintextMap& secret_map,
    CompPeerSeq& comp_peers) {

  for(auto pair : secret_map) {
    distribute_secret(pair.first, pair.second, comp_peers);
  }

}



template<class CompPeerSeq>
void InputPeer::bitwise_share(string key, int64_t value, CompPeerSeq& comp_peers) {

  size_t size = 64;
  boost::dynamic_bitset<> bs(size, value);

  string bits;
  for(auto i = 0; i < size; i++) {
    bits += lexical_cast<string>( bs[i] );
  }

  LOG4CXX_INFO(logger_, "Sharing bitset: " << bits);

  for(auto i = 0; i < size; i++) {
    LOG4CXX_INFO(logger_, "Sharing bit " << i << ": ");

    const string symbol = key + "b" + lexical_cast<string>(i);
    const auto bit = bs[i];

    secret_t secret(bit);
    auto shares = secret.share();
    distribute_shares(symbol, shares, bit, 0, comp_peers);
  }
}



template<class CompPeerSeq>
void InputPeer::lsb(
    string key,
    int64_t value,
    CompPeerSeq& comp_peers) {

  key = ".2" + key;
  value = 2 * value;

  int result = mod(value, PRIME);
  result = result % 2;

  LOG4CXX_INFO(logger_, "LSB (" << key << "): " << result);

  distribute_secret(key, result, comp_peers);
}



template<class CompPeerSeq>
vector<vertex_t> InputPeer::start_listeners(CompPeerSeq& comp_peers, graph_t& input_graph) {

  vector<vertex_t> nodes;

  auto iter = vertices(input_graph);
  auto last = iter.second;
  auto current = iter.first;

  for (; current != last; ++current) {
    const auto& current_vertex = *current;

    if (current_vertex < VERTEX_START) continue;
    if (current_vertex > VERTEX_END) continue;

    nodes.push_back(current_vertex);

    for(size_t i = 0; i < COMP_PEER_NUM; i++) {
      size_t port = 2000 + COMP_PEER_NUM*current_vertex + i;
      auto cp = comp_peers[i];
      auto sp = shared_ptr<RPCServer>(new RPCServer(cp->io_service_, port, cp.get() ) );
      for(auto ccp: comp_peers) {
        Vertex& vertex = ccp->bgp_->graph_[current_vertex];
        vertex.servers_[i] = sp;
      }
    }
  }

  return nodes;
}



template<class CompPeerSeq>
void InputPeer::start_clients(CompPeerSeq& comp_peers, graph_t& input_graph) {

  auto iter = vertices(input_graph);
  auto last = iter.second;
  auto current = iter.first;

  for (; current != last; ++current) {
    const auto& current_vertex = *current;

    if (current_vertex < VERTEX_START) continue;
    if (current_vertex > VERTEX_END) continue;

    for(size_t i = 0; i < COMP_PEER_NUM; i++) {
      size_t port = 2000 + COMP_PEER_NUM*current_vertex + i;

      for(size_t ID = 1; ID <= COMP_PEER_NUM; ID++) {

        if (COMP_PEER_IDS.find(ID) == COMP_PEER_IDS.end()) continue;

        auto cp = comp_peers[ID - 1];
        auto sp = shared_ptr<RPCClient>(new RPCClient(cp->io_service_, COMP_PEER_HOSTS[ID - 1], port));

        for(auto ccp: comp_peers) {
          Vertex& vertex = ccp->bgp_->graph_[current_vertex];
          vertex.clients_[ID][i] = sp;
        }

      }
    }
  }
}



template<class CompPeerSeq>
void InputPeer::disseminate_bgp(CompPeerSeq& comp_peers, graph_t& input_graph) {

  auto iter = vertices(input_graph);
  auto last = iter.second;
  auto current = iter.first;

  for (; current != last; ++current) {
    const auto& current_vertex = *current;
    Vertex& vertex = input_graph[current_vertex];

    for(const auto pair: vertex.preference_) {
      const auto key = pair.first;
      const auto value = pair.second;

      secret_t secret(value);
      auto shares = secret.share();

      for(size_t i = 0; i < COMP_PEER_NUM; i++) {

        string mpc_key;
        int64_t mpc_value;

        mpc_key = lexical_cast<string>(key);
        mpc_value = shares[i];

        value_map_t& vm = comp_peers[i]->bgp_->graph_[current_vertex].value_map_;
        vm[mpc_key] = mpc_value;

        mpc_key = ".2" + lexical_cast<string>(key);
        mpc_value = compute_lsb(2 * value);
        vm[mpc_key] = mpc_value;

        for(const auto pair: vertex.preference_) {
          const auto other_key = pair.first;
          const auto other_value = pair.second;

          mpc_key = ".2" + lexical_cast<string>(key) + "-" + lexical_cast<string>(other_key);
          mpc_value = compute_lsb(2 * (value - other_value));
          vm[mpc_key] = mpc_value;
        }
      }

    }

  }

  std::cout << "Connecting..." << std::endl;

  std::cout << "Done!" << std::endl;
}



#endif /* INPUT_PEER_TEMPLATE_HPP_ */
