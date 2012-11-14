
#ifndef COMP_PEER_TEMPLATE_HPP_
#define COMP_PEER_TEMPLATE_HPP_


#include <numeric>
#include <iostream>
#include <sstream>

#include <peer/comp_peer.hpp>
#include <boost/thread/locks.hpp>
#include <boost/assign.hpp>

template<const size_t Num>
Comp_peer<Num>::Comp_peer(size_t id, shared_ptr<Input_peer> input_peer) :
    Peer(id + 10000),
    id_(id),
    input_peer_(input_peer) {

  using boost::assign::list_of;

  recombination_array_ = (list_of(3), -3, 1);
  _gsl_vector_view view = gsl_vector_view_array(recombination_array_.begin(), Num);
  shared_ptr<gsl_vector> tmp_vector( gsl_vector_alloc(Num) );
  *tmp_vector = view.vector;

  recombination_vercor_ = tmp_vector;
}



template<const size_t Num>
void Comp_peer<Num>::generate_random_num(string key) {
  boost::random::uniform_int_distribution<> dist(-256, 256);
  const auto random = dist(rng_);

  const string id_string = lexical_cast<string>(id_);
  LOG4CXX_INFO( logger_, "Random seed (" << id_string << ") : " << random);


  barrier_mutex_.lock();

  distribute_secret(key + id_string, random, net_peers_);

  //cv_.wait(lock_);
  barrier_mutex_.lock();
  barrier_mutex_.unlock();

  std::stringstream stream;
  int64_t sum = 0;
  for(auto i = 1; i <= Num; i++) {
    const int64_t value = values_[key + lexical_cast<string>(i)];
    sum += value;

    stream << value << " ";
  }

  stream << " = " << sum;
  LOG4CXX_INFO( logger_,  id_ << ": " << stream.str());

  values_[key] = sum;
}




template<const size_t Num>
void Comp_peer<Num>::multiply(
    string first,
    string second,
    string recombine_key) {

  string key = recombine_key + "_" + boost::lexical_cast<string>(id_);
  int64_t result = values_[first] * values_[second];

  LOG4CXX_INFO( logger_,id_ << ": Mul " << values_[first] << " * " << values_[second] << " = " << result);

  barrier_mutex_.lock();

  distribute_secret(key, result, net_peers_);
  //cv_.wait(lock_);

  barrier_mutex_.lock();
  barrier_mutex_.unlock();

  recombine(recombine_key);
}


template<const size_t Num>
void Comp_peer<Num>::recombine(string recombination_key) {

  shared_ptr<gsl_vector> ds( gsl_vector_alloc(3) );

  std::stringstream ss;
  ss << id_ << ": ";
  for(size_t i = 0; i < Num; i++) {
    const string key = recombination_key + "_" + lexical_cast<string>(i + 1);
    gsl_vector_set(ds.get(), i, values_[key] );
    ss << values_[key] << " ";
  }

  shared_ptr<const gsl_vector> ds_const = ds;
  double recombine;

  gsl_blas_ddot(ds_const.get(), recombination_vercor_.get(), &recombine);
  values_[recombination_key] = recombine;
  ss << "= " << recombine;
  LOG4CXX_INFO(logger_, id_ << ": recombine: " << ss.str());
}



template<const size_t Num>
void Comp_peer<Num>::generate_random_bit(string key) {

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
    LOG4CXX_TRACE( logger_, id_ << ": (" << x[index] << ", " << y[index] << ")");
  }

  gsl_poly_dd_init( d, x, y, 3 );
  double interpol = gsl_poly_dd_eval( d, x, 3, 0);

  BOOST_ASSERT_MSG(interpol != 0, "pow(r, 2) != 0");
  LOG4CXX_TRACE( logger_, id_ << ": pow(r, 2)" << " = " << interpol);

  interpol = sqrt(interpol);

  BOOST_ASSERT_MSG(interpol > 0, "0 < r < p/2");
  BOOST_ASSERT_MSG(interpol < PRIME/2, "0 < r < p/2");

  const double bit = (rand/interpol + 1)/2;

  LOG4CXX_INFO( logger_, id_ << ": square" << " = " << interpol);
  LOG4CXX_INFO( logger_, id_ << ": random bit" << " = " << bit);
}



template<const size_t Num>
void Comp_peer<Num>::execute(vector<string> circut) {

  string first_operand = circut.back();
  circut.pop_back();

  string second_operand = circut.back();
  circut.pop_back();

  string operation = circut.back();
  circut.pop_back();

  recombination_key_ = first_operand + operation + second_operand;

  if (operation == "*") {
    multiply(first_operand, second_operand, recombination_key_);
  } else if (operation == "+") {
    add(first_operand, second_operand);
  } else {
    BOOST_ASSERT_MSG(false, "Operation not supported!");
  }

  const int64_t result = values_[recombination_key_];
  const string key = recombination_key_ + boost::lexical_cast<string>(id_);
  continue_or_not(circut, key, result);
}



template<const size_t Num>
void Comp_peer<Num>::add(
    string first,
    string second) {

  int64_t result = values_[first] + values_[second];
  result = mod(result, PRIME);

  const string key = recombination_key_ + lexical_cast<string>(id_);
  values_[recombination_key_] = result;
}



template<const size_t Num>
void Comp_peer<Num>::continue_or_not(
    vector<string> circut,
    const string key,
    const  int64_t result) {

  if(circut.empty()) {
    input_peer_->recombination_key_ = recombination_key_;
    input_peer_->publish(key, result);
  } else {
    circut.push_back(recombination_key_);
    execute(circut);
  }

}


#endif
