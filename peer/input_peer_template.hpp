  #ifndef INPUT_PEER_TEMPLATE_HPP_
#define INPUT_PEER_TEMPLATE_HPP_


#include <common.hpp>
#include <peer/input_peer.hpp>

#include <gsl/gsl_interp.h>
#include <gsl/gsl_poly.h>

#include <bitset>
#include <boost/dynamic_bitset.hpp>

typedef Secret<plaintext_t, COMP_PEER_NUM> secret_t;



template<size_t Num>
void InputPeer::result() {

  barrier_mutex_.lock();

  double x[Num], y[Num], d[Num];

  print_values();

  for(size_t i = 0; i < Num; i++) {
    std::string key = recombination_key_  + boost::lexical_cast<std::string>(i + 1);
    const auto value = values_[key];
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
}



template<class CompPeerSeq>
void InputPeer::distribute_secret(
    symbol_t key, plaintext_t value,
    CompPeerSeq& comp_peers) {

  secret_t secret(value);
  auto shares = secret.share();
  distribute_shares(key, shares, value, comp_peers);

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
    distribute_shares(symbol, shares, bit, comp_peers);
  }

}


template<class CompPeerSeq>
void InputPeer::lsb(
    string key,
    int64_t value,
    CompPeerSeq& comp_peers) {

  key = string(".2") + key;
  value = 2 * value;

  auto result = mod(value, PRIME);
  result = result % 2;

  LOG4CXX_INFO(logger_, "LSB (" << key << "): " << result);

  distribute_secret(key, result, comp_peers);
}


#endif /* INPUT_PEER_TEMPLATE_HPP_ */
