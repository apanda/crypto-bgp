
#ifndef COMP_PEER_TEMPLATE_HPP_
#define COMP_PEER_TEMPLATE_HPP_

#include <peer/comp_peer.hpp>
#include <boost/thread/locks.hpp>

template<const size_t Num>
Comp_peer<Num>::Comp_peer(size_t id, shared_ptr<Input_peer> input_peer) :
    Peer(id + 10000),
    id_(id),
    input_peer_(input_peer) {

  shared_ptr<gsl_vector> x( gsl_vector_alloc(Num) );

  gsl_vector_set(x.get(), 0, 3);
  gsl_vector_set(x.get(), 1, -3);
  gsl_vector_set(x.get(), 2, 1);

  recombination_vercor_ = x;
}



template<const size_t Num>
void Comp_peer<Num>::generate_random_num() {
  //boost::random::uniform_int_distribution<> dist(0, PRIME/Num);
  boost::random::uniform_int_distribution<> dist(0, 256);
  const auto random = dist(rng_);
  //const auto random = id_;

  LOG4CXX_INFO( logger_, "Random seed ("
      << lexical_cast<string>(id_) << ") : " << random);

  const string key = "RAND";

  barrier_mutex_.lock();
  distribute_secret(key + lexical_cast<string>(id_), random, net_peers_);
  barrier_mutex_.lock();
  barrier_mutex_.unlock();

  int sum = 0;
  for(auto i = 1; i <= Num; i++) {
    auto value = values_[key + lexical_cast<string>(i)];
    sum += value;
  }

  sum = mod(sum, PRIME);

  values_[key] = sum;
}



template<const size_t Num>
void Comp_peer<Num>::generate_random_bit() {

  generate_random_num();

  string key = "RAND";
  const double rand = values_[key];
  LOG4CXX_INFO( logger_, "Random generation done: " << rand);

  recombination_key_ = "RAND*RAND";

  multiply(key, key);

  key = "RAND*RAND";

  const auto result = values_[recombination_key_];

  barrier_mutex_.lock();

  for(int i = 0; i < COMP_PEER_NUM; i++) {
    net_peers_[i]->publish(key + lexical_cast<string>(id_), result);
  }

  barrier_mutex_.lock();
  barrier_mutex_.unlock();

  double x[Num], y[Num], d[Num];

  for(int i = 1; i <= Num; i++) {
    const size_t index = i - 1;
    x[index] = i;
    y[index] = values_[recombination_key_ + lexical_cast<string>(i)];
    LOG4CXX_INFO( logger_, id_ << ": " << x[index] << ", " << y[index]);
  }

  gsl_poly_dd_init( d, x, y, 3 );
  double interpol = gsl_poly_dd_eval( d, x, 3, 0);
  interpol = sqrt(interpol);
  //interpol = mod(interpol, PRIME);

  double bit = (rand/interpol + 1)/2;
  bool t = interpol < (PRIME / 2);

  LOG4CXX_INFO( logger_, "Bool: " << lexical_cast<string>(id_) << ": " << t);
  LOG4CXX_INFO( logger_, "Square: " << lexical_cast<string>(id_) << ": " << interpol);
  LOG4CXX_INFO( logger_, "Random bit: " << lexical_cast<string>(id_) << ": " << bit);

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
    multiply(first_operand, second_operand);
  } else if (operation == "+") {
    add(first_operand, second_operand);
  } else {
    BOOST_ASSERT_MSG(false, "Operation not supported!");
  }

  const int result = values_[recombination_key_];
  const string key = recombination_key_ + boost::lexical_cast<string>(id_);
  continue_or_not(circut, key, result);
}



template<const size_t Num>
void Comp_peer<Num>::add(
    string first,
    string second) {

  int result = values_[first] + values_[second];
  result = mod(result, PRIME);

  const string key = recombination_key_ + lexical_cast<string>(id_);
  values_[recombination_key_] = result;
}



template<const size_t Num>
void Comp_peer<Num>::continue_or_not(
    vector<string> circut,
    const string key,
    const int result) {

  if(circut.empty()) {
    input_peer_->recombination_key_ = recombination_key_;
    input_peer_->publish(key, result);
  } else {
    circut.push_back(recombination_key_);
    execute(circut);
  }

}



template<const size_t Num>
void Comp_peer<Num>::multiply(
    string first,
    string second) {

  const string key = recombination_key_ + boost::lexical_cast<string>(id_);
  int result = values_[first] * values_[second];
  LOG4CXX_TRACE( logger_, "Mul: " << result);
  result = mod(result, PRIME);

  barrier_mutex_.lock();

  distribute_secret(key, result, net_peers_);
  recombine();
}



template<const size_t Num>
void Comp_peer<Num>::recombine() {

  barrier_mutex_.lock();
  barrier_mutex_.unlock();

  shared_ptr<gsl_vector> ds( gsl_vector_alloc(3) );

  gsl_vector_set(ds.get(), 0, values_[recombination_key_ + "1"]);
  gsl_vector_set(ds.get(), 1, values_[recombination_key_ + "2"]);
  gsl_vector_set(ds.get(), 2, values_[recombination_key_ + "3"]);

  shared_ptr<const gsl_vector> ds_const = ds;
  double recombine;

  gsl_blas_ddot(ds_const.get(), recombination_vercor_.get(), &recombine);
  const int result = mod(recombine, PRIME);
  values_[recombination_key_] = result;
}

#endif
