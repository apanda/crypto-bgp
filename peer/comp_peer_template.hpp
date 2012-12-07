
#ifndef COMP_PEER_TEMPLATE_HPP_
#define COMP_PEER_TEMPLATE_HPP_

#include <peer/comp_peer.hpp>

#include <numeric>
#include <iostream>
#include <sstream>

#include <boost/thread/locks.hpp>
#include <boost/assign.hpp>

template<const size_t Num>
CompPeer<Num>::CompPeer(
    size_t id,
    shared_ptr<InputPeer> input_peer,
    std::unordered_map<int, shared_ptr<boost::barrier> > b,
    io_service& io) :
      Peer(id + 10000, io),
      id_(id),
      input_peer_(input_peer),
      bgp_(new BGPProcess("scripts/dot.dot", b[300], this)),
      barrier_map_(b)
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

  //const double comp = compare(a, b);

}


template<const size_t Num>
void CompPeer<Num>::evaluate(vector<string> circut, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  const symbol_t recombination_key = execute(circut, l);
  const int64_t result = vlm[recombination_key];
  LOG4CXX_INFO( logger_, "Return value: " << recombination_key);

  input_peer_->recombination_key_ = recombination_key;
  input_peer_->publish(recombination_key + lexical_cast<string>(id_), result, l);
}



template<const size_t Num>
symbol_t CompPeer<Num>::execute(vector<string> circut, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

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
      multiply_const(first_operand, number, recombination_key, l);
    } catch(boost::bad_lexical_cast& e) {
      multiply(first_operand, second_operand, recombination_key, l);
    }

  } else if (operation == "+") {
    add(first_operand, second_operand, recombination_key, l);
  } else if (operation == "-") {
    sub(first_operand, second_operand, recombination_key, l);
  } else {
    BOOST_ASSERT_MSG(false, "Operation not supported!");
  }

  const int64_t result = vlm[recombination_key];
  const string key = recombination_key + boost::lexical_cast<string>(id_);

  std::string final = continue_or_not(circut, key, result, recombination_key, l);
  return final;
}



template<const size_t Num>
symbol_t CompPeer<Num>::add(
    string first,
    string second,
    string recombination_key, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  int64_t result = vlm[second] + vlm[first];
  result = mod(result, PRIME);

  const string key = recombination_key + lexical_cast<string>(id_);
  vlm[recombination_key] = result;
  return recombination_key;
}



template<const size_t Num>
symbol_t CompPeer<Num>::sub(
    string first,
    string second,
    string recombination_key, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  int64_t result = vlm[first] - vlm[second];
  result = mod(result, PRIME);

  const string key = recombination_key + lexical_cast<string>(id_);
  vlm[recombination_key] = result;
  return recombination_key;
}





template<const size_t Num>
symbol_t CompPeer<Num>::generate_random_num(string key, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  boost::random::uniform_int_distribution<> dist(-256, 256);
  const auto random = dist(rng_);

  const string id_string = lexical_cast<string>(id_);
  LOG4CXX_DEBUG( logger_, id_string << ": random seed: " << random);

  mutex_map_[l]->lock();

  distribute_secret(key + id_string, l, random, net_peers_);

  mutex_map_[l]->lock();
  mutex_map_[l]->unlock();

  int64_t sum = 0;
  for(auto i = 1; i <= Num; i++) {
    const int64_t value = vlm[key + lexical_cast<string>(i)];
    sum += value;
  }

  LOG4CXX_DEBUG( logger_,  id_ << "Sum: " << sum);

  vlm[key] = sum;
  return key;
}



template<const size_t Num>
int CompPeer<Num>::compare(string key1, string key2, vertex_t l) {


  barrier_map_[l]->wait();


  value_map_t& vlm = vertex_value_map_[l];

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  LOG4CXX_DEBUG( logger_, id_ << ": w: " << vlm[w]);
  LOG4CXX_DEBUG( logger_, id_ << ": x: " << vlm[x]);
  LOG4CXX_DEBUG( logger_, id_ << ": y: " << vlm[y]);

  vector<string> wx_cricut = {"*", w, x};
  const string wx( execute(wx_cricut, l) );
  LOG4CXX_DEBUG( logger_,  id_ << ": wx: " << wx << ": " << vlm[wx]);

  boost::this_thread::yield();
  barrier_map_[l]->wait();

  vector<string> wy_cricut = {"*", w, y};
  const string wy( execute(wy_cricut, l) );
  LOG4CXX_DEBUG( logger_,  id_ << ": wy: " << wy << ": " << vlm[wy]);

  boost::this_thread::yield();
  barrier_map_[l]->wait();

  vector<string> wxy2_cricut = {"*", "2", "*", y, "*", w, x};
  const string wxy2( execute(wxy2_cricut, l) );
  LOG4CXX_DEBUG( logger_,  id_ << ": 2wxy: " << wxy2 << ": " << vlm[wxy2]);

  boost::this_thread::yield();
  barrier_map_[l]->wait();

  vector<string> xy_cricut = {"*", x, y};
  const string xy( execute(xy_cricut, l) );
  LOG4CXX_DEBUG( logger_,  id_ << ": xy: " << xy << ": " << vlm[xy]);

  boost::this_thread::yield();
  barrier_map_[l]->wait();

  vector<string> final = {
     "+", xy, "-", x, "-", y, "-", wxy2, "+", wy, wx
  };

  string result = execute(final, l);

  boost::this_thread::yield();
  barrier_map_[l]->wait();

  auto value = vlm[result] + 1;
  value = mod(value, PRIME);

  mutex_map_[l]->lock();

  Vertex& v = bgp_->graph_[l];

  for(size_t i = 0; i < COMP_PEER_NUM; i++) {
    v.clients_[id_][i]->publish(result + lexical_cast<string>(id_), value, l);
  }

  boost::this_thread::yield();

  mutex_map_[l]->lock();
  mutex_map_[l]->unlock();

  barrier_map_[l]->wait();

  double X[Num], Y[Num], D[Num];

  //print_values();

  for(size_t i = 0; i < Num; i++) {
    std::string key = result  + boost::lexical_cast<std::string>(i + 1);
    const auto value = vlm[key];
    intermediary_[i + 1] = value;
  }

  for(auto it = intermediary_.begin(); it != intermediary_.end(); ++it) {
    const auto _x = it->first;
    const auto _y = it->second;
    const auto _index = it->first - 1;

    X[_index] = _x;
    Y[_index] = _y;
  }

  gsl_poly_dd_init( D, X, Y, 3 );
  const double interpol = gsl_poly_dd_eval( D, X, 3, 0);

  return mod(interpol, PRIME);
}



template<const size_t Num>
symbol_t CompPeer<Num>::generate_random_bitwise_num(string key, vertex_t l) {

  for(auto i = 0; i < SHARE_BIT_SIZE; i++) {
    const string bit_key = key + "b" + lexical_cast<string>(i);
    generate_random_bit(bit_key, l);
    counter_ = 0;
  }

  return key;
}



template<const size_t Num>
symbol_t CompPeer<Num>::multiply_const(
    string first,
    int64_t second,
    string recombination_key, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  vlm.at(first);
  const auto result = vlm[first] * second;
  vlm.insert( make_pair(recombination_key, result) );

  return recombination_key;
}



template<const size_t Num>
symbol_t CompPeer<Num>::multiply(
    string first,
    string second,
    string recombination_key, vertex_t l) {

  LOG4CXX_TRACE( logger_, "CompPeer<Num>::multiply -> " << l);
  value_map_t& vlm = vertex_value_map_[l];

  vlm.at(first);
  vlm.at(second);

  const string key = recombination_key + "_" + boost::lexical_cast<string>(id_);
  const int64_t result = vlm[first] * vlm[second];

  mutex_map_[l]->lock();

  Vertex& v = bgp_->graph_[l];
  distribute_secret(key, result, l, v.clients_[id_]);
  //distribute_secret(key, result, l, net_peers_);

  boost::this_thread::yield();

  mutex_map_[l]->lock();
  mutex_map_[l]->unlock();

  return recombine(recombination_key, l);
}



template<const size_t Num>
symbol_t CompPeer<Num>::recombine(string recombination_key, vertex_t l) {

  vertex_value_map_.at(l);
  value_map_t& vlm = vertex_value_map_[l];

  gsl_vector* ds = gsl_vector_alloc(3);

  LOG4CXX_TRACE( logger_, "CompPeer<Num>::recombine -> " << l);

  std::stringstream debug_stream;
  for(size_t i = 0; i < Num; i++) {
    const string key = recombination_key + "_" + lexical_cast<string>(i + 1);

    try {
      vlm.at(key);
    } catch (...) {
      string error = "recombine: " +  key;

      for(auto val: vlm) {
        printf("\t%s: %ld\n", val.first.c_str(), val.second);
      }

      throw std::runtime_error(error);


    }

    const int64_t value = vlm[key];

    gsl_vector_set(ds, i,  value);
    debug_stream <<  " " << value;
  }

  double recombine;

  gsl_blas_ddot(ds, recombination_vercor_, &recombine);

  vlm.insert(make_pair(recombination_key, recombine));
  debug_stream << ": " << recombine;
  LOG4CXX_TRACE(logger_, id_ << ": recombine:" << debug_stream.str());

  gsl_vector_free(ds);

  return recombination_key;
}



template<const size_t Num>
symbol_t CompPeer<Num>::generate_random_bit(string key, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  generate_random_num(key, l);
  const double rand = vlm[key];

  string recombination_key = key + "*" + key;

  multiply(key, key, recombination_key, l);

  const auto result = vlm[recombination_key];

  mutex_map_[l]->lock();

  for( int64_t i = 0; i < COMP_PEER_NUM; i++) {
    net_peers_[i]->publish(recombination_key + lexical_cast<string>(id_), result), l;
  }

  //cv_.wait(lock_);
  mutex_map_[l]->lock();
  mutex_map_[l]->unlock();

  double x[Num], y[Num], d[Num];

  for( int64_t i = 1; i <= Num; i++) {
    const size_t index = i - 1;
    x[index] = i;
    y[index] = vlm[recombination_key + lexical_cast<string>(i)];
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
    string recombination_key, vertex_t l) {

  if(circut.empty()) {
    return recombination_key;
  } else {
    circut.push_back(recombination_key);
    return execute(circut, l);
  }

}


#endif
