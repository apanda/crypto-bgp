
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
      bgp_(new BGPProcess("scripts/dot.dot", b[300], this, io_service_)),
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
void CompPeer<Num>::execute(vector<string> circut, vertex_t l) {

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

  continue_or_not(circut, key, result, recombination_key, l);
}



template<const size_t Num>
void CompPeer<Num>::add(
    string first,
    string second,
    string recombination_key, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  int64_t result = vlm[second] + vlm[first];
  result = mod(result, PRIME);

  const string key = recombination_key + lexical_cast<string>(id_);
  vlm[recombination_key] = result;
}



template<const size_t Num>
void CompPeer<Num>::sub(
    string first,
    string second,
    string recombination_key, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  int64_t result = vlm[first] - vlm[second];
  result = mod(result, PRIME);

  const string key = recombination_key + lexical_cast<string>(id_);
  vlm[recombination_key] = result;
}





template<const size_t Num>
void CompPeer<Num>::generate_random_num(string key, vertex_t l) {

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
}




template<const size_t Num>
void CompPeer<Num>::compare0(string key1, string key2, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  LOG4CXX_INFO( logger_, id_ << " (" << l <<  ") " << ": w: " << vlm[w]);
  LOG4CXX_INFO( logger_, id_ << " (" << l <<  ") " << ": x: " << vlm[x]);
  LOG4CXX_INFO( logger_, id_ << " (" << l <<  ") " << ": y: " << vlm[y]);

  vector<string> circut = {"*", w, x};
  string circut_str = x + "*" + w;


  sig_map_0x[l][circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(sig_map_0x[l][circut_str]) =
      boost::bind(&CompPeer<Num>::compare1, this, key1, key2, l);


  sig_map_x[l][circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(sig_map_x[l][circut_str]) =
      boost::bind(&CompPeer<Num>::recombine, this, circut_str, l);

  execute(circut, l);
}



template<const size_t Num>
void CompPeer<Num>::compare1(string key1, string key2, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  vector<string> circut = {"*", w, y};
  string circut_str = y + "*" + w;

  string wx = x + "*" + w;
  LOG4CXX_INFO( logger_,  id_ << ": wx: " << wx << ": " << vlm[wx]);

  sig_map_0x[l][circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(sig_map_0x[l][circut_str]) =
      boost::bind(&CompPeer<Num>::compare2, this, key1, key2, l);


  sig_map_x[l][circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(sig_map_x[l][circut_str]) =
      boost::bind(&CompPeer<Num>::recombine, this, circut_str, l);

  execute(circut, l);
}


template<const size_t Num>
void CompPeer<Num>::compare2(string key1, string key2, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  vector<string> circut = {"*", x, y};
  string circut_str = y + "*" + x;
  string wy = y + "*" + w;

  LOG4CXX_INFO( logger_,  id_ << ": wy: " << wy << ": " << vlm[wy]);

  sig_map_0x[l][circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(sig_map_0x[l][circut_str]) =
      boost::bind(&CompPeer<Num>::compare3, this, key1, key2, l);


  sig_map_x[l][circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(sig_map_x[l][circut_str]) =
      boost::bind(&CompPeer<Num>::recombine, this, circut_str, l);

  execute(circut, l);
}



template<const size_t Num>
void CompPeer<Num>::compare3(string key1, string key2, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  string xy = y + "*" + x;

  vector<string> circut = {"*", w, "*", "2", xy};
  string circut_str = xy + "*" + "2" + "*" + w;

  LOG4CXX_INFO( logger_,  id_ << ": xy: " << xy << ": " << vlm[xy]);
  sig_map_0x[l][circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(sig_map_0x[l][circut_str]) =
      boost::bind(&CompPeer<Num>::compare4, this, key1, key2, l);


  sig_map_x[l][circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(sig_map_x[l][circut_str]) =
      boost::bind(&CompPeer<Num>::recombine, this, circut_str, l);

  execute(circut, l);
}



template<const size_t Num>
void CompPeer<Num>::compare4(string key1, string key2, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  string xy = y + "*" + x;
  string wx = x + "*" + w;
  string wy = y + "*" + w;

  string wxy2 = xy + "*" + "2" + "*" + w;

  std::vector<string> circut = {"+", xy, "-", x, "-", y, "-", wxy2, "+", wy, wx};
  string circut_str = wx + "+" + wy + "-" + wxy2 + "-" + y + "-" + x + "+" + xy;

  LOG4CXX_INFO( logger_,  id_ << ": 2xyw: " << wxy2 << ": " << vlm[wxy2]);


  sig_map_0x[l][circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(sig_map_0x[l][circut_str]) =
      boost::bind(&CompPeer<Num>::compare5, this, key1, key2, l);


  sig_map_x[l][circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(sig_map_x[l][circut_str]) =
      boost::bind(&CompPeer<Num>::recombine, this, circut_str, l);

  execute(circut, l);

  LOG4CXX_INFO( logger_,  id_ << ": Final: " << ": " << vlm[circut_str] );


  auto value = vlm[circut_str] + 1;
  value = mod(value, PRIME);

  Vertex& v = bgp_->graph_[l];

  for(size_t i = 0; i < COMP_PEER_NUM; i++) {
    v.clients_[id_][i]->publish(circut_str + "_" + lexical_cast<string>(id_), value, l);
  }

}



template<const size_t Num>
void CompPeer<Num>::compare5(string key1, string key2, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  string xy = y + "*" + x;
  string wx = x + "*" + w;
  string wy = y + "*" + w;

  string wxy2 = xy + "*" + "2" + "*" + w;
  string result = wx + "+" + wy + "-" + wxy2 + "-" + y + "-" + x + "+" + xy;

  double X[Num], Y[Num], D[Num];

  for(size_t i = 0; i < Num; i++) {
    std::string key = result + "_"  + boost::lexical_cast<std::string>(i + 1);
    const auto value = vlm[key];
    intermediary_[i + 1] = value;

    //LOG4CXX_INFO( logger_, "intermediary_: " << key << ": " << value);
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

  const double end = mod(interpol, PRIME);
  LOG4CXX_INFO( logger_, "Result: " << ": " << end);


  sig_map_2x[l][result]->operator ()(( end ));

}



template<const size_t Num>
void CompPeer<Num>::generate_random_bitwise_num(string key, vertex_t l) {

  for(auto i = 0; i < SHARE_BIT_SIZE; i++) {
    const string bit_key = key + "b" + lexical_cast<string>(i);
    generate_random_bit(bit_key, l);
    counter_ = 0;
  }
}



template<const size_t Num>
void CompPeer<Num>::multiply_const(
    string first,
    int64_t second,
    string recombination_key, vertex_t l) {

  value_map_t& vlm = vertex_value_map_[l];

  vlm.at(first);
  const auto result = vlm[first] * second;
  vlm.insert( make_pair(recombination_key, result) );

  mutex_map_2[l].insert(
      make_pair(recombination_key, shared_ptr<mutex_t>(new mutex_t))
      );

  cv_map_2[l].insert(
      make_pair(recombination_key, shared_ptr<condition_variable_t>(new condition_variable_t))
      );
}



template<const size_t Num>
void CompPeer<Num>::multiply(
    string first,
    string second,
    string recombination_key, vertex_t l) {

  LOG4CXX_TRACE( logger_, "CompPeer<Num>::multiply");

  value_map_t& vlm = vertex_value_map_[l];

  vlm.at(first);
  vlm.at(second);

  const string key = recombination_key + "_" + lexical_cast<string>(id_);
  const int64_t result = vlm[first] * vlm[second];

  Vertex& v = bgp_->graph_[l];
  distribute_secret(key, result, l, v.clients_[id_]);
}



template<const size_t Num>
void CompPeer<Num>::recombine(string recombination_key, vertex_t l) {

  LOG4CXX_TRACE( logger_, "CompPeer<Num>::recombine -> " << l);

  vertex_value_map_.at(l);
  value_map_t& vlm = vertex_value_map_[l];

  gsl_vector* ds = gsl_vector_alloc(3);

  std::stringstream debug_stream;
  for(size_t i = 0; i < Num; i++) {
    const string key = recombination_key + "_" + lexical_cast<string>(i + 1);

    try {
      vlm.at(key);
    } catch (...) {
      string error = "recombine: " +  key;
      printf("( %s )\n", error.c_str());

      for(auto val: vlm) {

      }

      //throw std::runtime_error(error);

    }

    const int64_t value = vlm[key];

    gsl_vector_set(ds, i,  value);
    debug_stream <<  " " << value;
  }

  double recombine;

  gsl_blas_ddot(ds, recombination_vercor_, &recombine);

  vlm[recombination_key] = recombine;
  //vlm.insert(make_pair(recombination_key, recombine));
  debug_stream << ": " << recombine;
  LOG4CXX_DEBUG(logger_, id_ << ": recombine:" << debug_stream.str());

  gsl_vector_free(ds);


  sig_map_0x[l][recombination_key]->operator ()();
}



template<const size_t Num>
void CompPeer<Num>::generate_random_bit(string key, vertex_t l) {

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
}



template<const size_t Num>
void CompPeer<Num>::continue_or_not(
    vector<string> circut,
    const string key,
    const int64_t result,
    string recombination_key, vertex_t l) {

  if(circut.empty()) {
  } else {
    circut.push_back(recombination_key);
    execute(circut, l);
  }

}


#endif
