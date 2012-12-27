#ifndef INPUT_PEER_TEMPLATE_HPP_
#define INPUT_PEER_TEMPLATE_HPP_

#include <common.hpp>
#include <bgp/bgp.hpp>

#include <bitset>

#include <gsl/gsl_interp.h>
#include <gsl/gsl_poly.h>

#include <boost/dynamic_bitset.hpp>


typedef Secret<plaintext_t, COMP_PEER_NUM> secret_t;


template<size_t Num>
void InputPeer::result() {
/*
  barrier_mutex_.lock();

  double x[Num], y[Num], d[Num];

  print_values();

  for(size_t i = 0; i < Num; i++) {
    std::string key = recombination_key_  + boost::lexical_cast<std::string>(i + 1);
    const auto value = (*values_)[key];
    intermediary_[i + 1] = value;
  }

  for(auto it = intermediary_.begin(); it != intermediary_.end(); ++it) {
    const auto _x = it->first;
    const auto _y = it->second;
    const auto _index = it->first - 1;

    x[_index] = _x;
    y[_index] = _y;
  }



  gsl_poly_dd_init( d, x, y, 3 );
  const double interpol = gsl_poly_dd_eval( d, x, 3, 0);

  LOG4CXX_INFO(logger_, "Result: " << interpol);
  LOG4CXX_INFO(logger_, "Result: " << mod(interpol, PRIME) );
  */
}



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
void InputPeer::disseminate_bgp(CompPeerSeq& comp_peers) {

  boost::shared_ptr<boost::barrier> bp;
  BGPProcess bgp("scripts/dot.dot", NULL, io_service_);

  graph_t& input_graph = bgp.graph_;

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
        //Vertex& tmp_vertex = comp_peers[i]->bgp_->graph_[current_vertex];

        string mpc_key;
        int64_t mpc_value;

        mpc_key = lexical_cast<string>(key);
        mpc_value = shares[i];

        value_map_t& vm = comp_peers[i]->bgp_->graph_[current_vertex].value_map_;
        //value_map_t& vm = *(tmp_vertex.values_);
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



  iter = vertices(input_graph);
  last = iter.second;
  current = iter.first;

  for (; current != last; ++current) {
    const auto& current_vertex = *current;

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

  std::cout << "Connecting..." << std::endl;

  iter = vertices(input_graph);
  last = iter.second;
  current = iter.first;

  for (; current != last; ++current) {
    const auto& current_vertex = *current;

    for(size_t i = 0; i < COMP_PEER_NUM; i++) {
      size_t port = 2000 + COMP_PEER_NUM*current_vertex + i;
      for(size_t ID = 0; ID < COMP_PEER_NUM; ID++) {
        auto cp = comp_peers[ID];
        auto sp = shared_ptr<RPCClient>(new RPCClient(cp->io_service_, "localhost", port ));
        for(auto ccp: comp_peers) {
          Vertex& vertex = ccp->bgp_->graph_[current_vertex];
          vertex.clients_[ID + 1][i] = sp;

        }
      }
    }
  }

  std::cout << "Done!" << std::endl;
}



#endif /* INPUT_PEER_TEMPLATE_HPP_ */
