
#ifndef COMP_PEER_TEMPLATE_HPP_
#define COMP_PEER_TEMPLATE_HPP_

#include <peer/comp_peer.hpp>

#include <numeric>
#include <iostream>
#include <sstream>

#include <boost/thread/locks.hpp>
#include <boost/assign.hpp>

template<const size_t Num>
CompPeer<Num>::CompPeer(size_t id, shared_ptr<InputPeer> input_peer) :
    Peer(id + 10000),
    id_(id),
    input_peer_(input_peer),
    bgp_(new BGPProcess("scripts/dot.dot", this))
    {

  using boost::assign::list_of;

  recombination_array_ = (list_of(3), -3, 1);
  _gsl_vector_view view = gsl_vector_view_array(recombination_array_.begin(), Num);
  gsl_vector* tmp_vector( gsl_vector_alloc(Num) );
  *tmp_vector = view.vector;

  recombination_vercor_ = tmp_vector;
}



template<const size_t Num>
CompPeer<Num>::~CompPeer() {
  gsl_vector_free(recombination_vercor_);
}



template<const size_t Num>
void CompPeer<Num>::evaluate(string a, string b) {

  const string comp = compare(a, b);

}


template<const size_t Num>
void CompPeer<Num>::evaluate(vector<string> circut) {

  const symbol_t recombination_key = execute(circut);
  const int64_t result = values_[recombination_key];
  LOG4CXX_INFO( logger_, "Return value: " << recombination_key);

  input_peer_->recombination_key_ = recombination_key;
  input_peer_->publish(recombination_key + lexical_cast<string>(id_), result);
}



template<const size_t Num>
symbol_t CompPeer<Num>::execute(vector<string> circut) {

  const string first_operand = circut.back();
  circut.pop_back();

  const string second_operand = circut.back();
  circut.pop_back();

  const string operation = circut.back();
  circut.pop_back();

  const string recombination_key = first_operand + operation + second_operand;

  if (operation == "*") {

    try {
      const int64_t number = lexical_cast<int>(second_operand);
      multiply_const(first_operand, number, recombination_key);
    } catch(boost::bad_lexical_cast& e) {
      multiply(first_operand, second_operand, recombination_key);
    }

  } else if (operation == "+") {
    add(first_operand, second_operand, recombination_key);
  } else if (operation == "-") {
    sub(first_operand, second_operand, recombination_key);
  } else {
    BOOST_ASSERT_MSG(false, "Operation not supported!");
  }

  const int64_t result = values_[recombination_key];
  const string key = recombination_key + boost::lexical_cast<string>(id_);

  return continue_or_not(circut, key, result, recombination_key);
}



template<const size_t Num>
symbol_t CompPeer<Num>::add(
    string first,
    string second,
    string recombination_key) {

  int64_t result = values_[second] + values_[first];
  result = mod(result, PRIME);

  const string key = recombination_key + lexical_cast<string>(id_);
  values_[recombination_key] = result;
  return recombination_key;
}



template<const size_t Num>
symbol_t CompPeer<Num>::sub(
    string first,
    string second,
    string recombination_key) {

  int64_t result = values_[first] - values_[second];
  result = mod(result, PRIME);

  const string key = recombination_key + lexical_cast<string>(id_);
  values_[recombination_key] = result;
  return recombination_key;
}





template<const size_t Num>
symbol_t CompPeer<Num>::generate_random_num(string key) {

  boost::random::uniform_int_distribution<> dist(-256, 256);
  const auto random = dist(rng_);

  const string id_string = lexical_cast<string>(id_);
  LOG4CXX_DEBUG( logger_, id_string << ": random seed: " << random);

  barrier_mutex_.lock();

  distribute_secret(key + id_string, random, net_peers_);

  barrier_mutex_.lock();
  barrier_mutex_.unlock();

  int64_t sum = 0;
  for(auto i = 1; i <= Num; i++) {
    const int64_t value = values_[key + lexical_cast<string>(i)];
    sum += value;
  }

  LOG4CXX_DEBUG( logger_,  id_ << "Sum: " << sum);

  values_[key] = sum;
  return key;
}



template<const size_t Num>
symbol_t CompPeer<Num>::compare(string key1, string key2) {

  string result;

  for(auto i = 0; i < 1000; i ++) {

    string w = ".2" + key1;
    string x = ".2" + key2;
    string y = ".2" + key1 + "-" + key2;

    LOG4CXX_TRACE( logger_, id_ << ": w: " << values_[w]);
    LOG4CXX_TRACE( logger_, id_ << ": x: " << values_[x]);
    LOG4CXX_TRACE( logger_, id_ << ": y: " << values_[y]);

    vector<string> wx_cricut = {"*", w, x};
    const string wx = execute(wx_cricut);
    LOG4CXX_TRACE( logger_,  id_ << ": wx: " << wx << ": " << values_[wx]);

    vector<string> wy_cricut = {"*", w, y};
    const string wy = execute(wy_cricut);
    LOG4CXX_TRACE( logger_,  id_ << ": wy: " << wy << ": " << values_[wy]);

    vector<string> wxy2_cricut = {"*", "2", "*", y, "*", w, x};
    const string wxy2 = execute(wxy2_cricut);
    LOG4CXX_TRACE( logger_,  id_ << ": 2wxy: " << wxy2 << ": " << values_[wxy2]);

    vector<string> xy_cricut = {"*", x, y};
    const string xy = execute(xy_cricut);
    LOG4CXX_TRACE( logger_,  id_ << ": xy: " << xy << ": " << values_[xy]);

    vector<string> final = {
       "+", xy, "-", x, "-", y, "-", wxy2, "+", wy, wx
    };

    result = execute(final);
  }

  auto value = values_[result] + 1;
  value = mod(value, PRIME);

  LOG4CXX_INFO(logger_,  id_ << ": result: " << ": " << mod(values_[result] + 1, PRIME));

  input_peer_->recombination_key_ = result;
  input_peer_->publish(result + lexical_cast<string>(id_), value);

  return result;
}



template<const size_t Num>
symbol_t CompPeer<Num>::generate_random_bitwise_num(string key) {

  for(auto i = 0; i < SHARE_BIT_SIZE; i++) {
    const string bit_key = key + "b" + lexical_cast<string>(i);
    generate_random_bit(bit_key);
    counter_ = 0;
  }

  return key;
}


template<const size_t Num>
symbol_t CompPeer<Num>::multiply_const(
    string first,
    int64_t second,
    string recombination_key) {

  const auto result = values_[first] * second;
  values_.insert( make_pair(recombination_key, result) );

  return recombination_key;
}



template<const size_t Num>
symbol_t CompPeer<Num>::multiply(
    string first,
    string second,
    string recombination_key) {

  //stringstream debug_stream;

  const string key = recombination_key + "_" + boost::lexical_cast<string>(id_);
  const int64_t result = values_[first] * values_[second];
  //debug_stream << values_[first] << " " << values_[second] << ": " << result;

  //LOG4CXX_DEBUG(logger_,id_ << ": multiply: " << debug_stream.str());

  barrier_mutex_.lock();

  distribute_secret(key, result, net_peers_);

  barrier_mutex_.lock();
  barrier_mutex_.unlock();

  return recombine(recombination_key);
}


template<const size_t Num>
symbol_t CompPeer<Num>::recombine(string recombination_key) {

  gsl_vector* ds = gsl_vector_alloc(3);

  std::stringstream debug_stream;
  for(size_t i = 0; i < Num; i++) {
    const string key = recombination_key + "_" + lexical_cast<string>(i + 1);
    const int64_t value = values_[key];

    gsl_vector_set(ds, i,  value);
    debug_stream <<  " " << value;
  }

  double recombine;

  gsl_blas_ddot(ds, recombination_vercor_, &recombine);
  values_[recombination_key] = recombine;
  debug_stream << ": " << recombine;
  LOG4CXX_DEBUG(logger_, id_ << ": recombine:" << debug_stream.str());

  gsl_vector_free(ds);

  return recombination_key;
}



template<const size_t Num>
symbol_t CompPeer<Num>::generate_random_bit(string key) {

  generate_random_num(key);
  const double rand = values_[key];

  string recombination_key = key + "*" + key;

  multiply(key, key, recombination_key);

  const auto result = values_[recombination_key];

  barrier_mutex_.lock();

  for( int64_t i = 0; i < COMP_PEER_NUM; i++) {
    net_peers_[i]->publish(recombination_key + lexical_cast<string>(id_), result);
  }

  //cv_.wait(lock_);
  barrier_mutex_.lock();
  barrier_mutex_.unlock();

  double x[Num], y[Num], d[Num];

  for( int64_t i = 1; i <= Num; i++) {
    const size_t index = i - 1;
    x[index] = i;
    y[index] = values_[recombination_key + lexical_cast<string>(i)];
  }

  gsl_poly_dd_init( d, x, y, 3 );
  double interpol = gsl_poly_dd_eval( d, x, 3, 0);

  LOG4CXX_TRACE( logger_, id_ << ": pow(r, 2)" << " = " << interpol);

  BOOST_ASSERT_MSG(interpol != 0, "pow(r, 2) != 0");

  interpol = sqrt(interpol);

  BOOST_ASSERT_MSG(interpol > 0, "0 < r < p/2");
  BOOST_ASSERT_MSG(interpol < PRIME/2, "0 < r < p/2");

  const double bit = (rand/interpol + 1)/2;

  LOG4CXX_DEBUG(logger_, id_ << ": r" << ": " << interpol);
  LOG4CXX_INFO(logger_, id_ << ": " << key << ": random bit" << ": " << bit);

  return key;
}



template<const size_t Num>
symbol_t CompPeer<Num>::continue_or_not(
    vector<string> circut,
    const string key,
    const int64_t result,
    string recombination_key) {

  if(circut.empty()) {
    return recombination_key;
  } else {
    circut.push_back(recombination_key);
    return execute(circut);
  }

}


#endif
