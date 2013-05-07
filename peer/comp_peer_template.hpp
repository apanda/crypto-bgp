
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
      Peer(io),
      bgp_(new BGPProcess("scripts/dot.dot", this, io_service_)),
      id_(id),
      input_peer_(input_peer),
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
void CompPeer<Num>::publish(std::string key, int64_t value, vertex_t v) {

  Vertex& vertex = bgp_->graph_[v];
  string rkey = key.substr(0, key.size() - 2);

  LOG4CXX_TRACE(logger_, " Acquired lock... " << v << ": " << rkey);


  value_map_t& vlm = vertex.value_map_;

  LOG4CXX_TRACE(logger_, "Received value: " << key << ": " << value << " (" << v << ")");

  vertex.mutex_->lock();
  vlm[key] = value;
  int& counter = vertex.couter_map_2[rkey];

  LOG4CXX_TRACE(logger_, "Counter... (" << v << "): " << rkey << " " << counter);

  counter++;

  if (counter == 3) {
    counter = 0;
    vertex.mutex_->unlock();
    LOG4CXX_TRACE(logger_, "Recombine...");
    vertex.sig_recombine[rkey]->operator()();
  } else if(counter > 3) {
    throw std::runtime_error("Should never throw!");
  } else {
    vertex.mutex_->unlock();
  }


}



template<const size_t Num>
void CompPeer<Num>::evaluate(vector<string> circut, vertex_t l) {

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  const symbol_t recombination_key = execute(circut, l);
  const int64_t result = vlm[recombination_key];
  LOG4CXX_DEBUG( logger_, "Return value: " << recombination_key);

  input_peer_->recombination_key_ = recombination_key;
  input_peer_->publish(recombination_key + lexical_cast<string>(id_), result, l);
}



template<const size_t Num>
void CompPeer<Num>::execute(vector<string> circut, vertex_t l) {

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  const string first_operand = circut.back();
  circut.pop_back();

  const string second_operand = circut.back();
  circut.pop_back();

  const string operation = circut.back();
  circut.pop_back();

  const string recombination_key = first_operand + operation + second_operand;

  if (operation == "*") {

    if (is_number(second_operand)) {
      /*
       * Warning: the number has to be positive!
       */
      const int64_t number = lexical_cast<int>(second_operand);
      multiply_const(first_operand, number, recombination_key, l);
    } else {
      multiply(first_operand, second_operand, recombination_key, l);
    }

  } else if (operation == "==") {
    multiply_eq(first_operand, recombination_key, l);

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

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  int64_t result = vlm[second] + vlm[first];
  LOG4CXX_DEBUG( logger_, first << "+" << second << "="<< result);
  LOG4CXX_DEBUG( logger_, vlm[first]  << "+" << vlm[second] << "="<< result);
  result = mod(result, PRIME);

  const string key = recombination_key + lexical_cast<string>(id_);
  vlm[recombination_key] = result;
}



template<const size_t Num>
void CompPeer<Num>::sub(
    string first,
    string second,
    string recombination_key, vertex_t l) {

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  int64_t result = vlm[first] - vlm[second];
  LOG4CXX_DEBUG( logger_, first << "-" << second << "="<< result);
  LOG4CXX_DEBUG( logger_, vlm[first]  << "-" << vlm[second] << "="<< result);
  result = mod(result, PRIME);

  const string key = recombination_key + lexical_cast<string>(id_);
  vlm[recombination_key] = result;
}



template<const size_t Num>
void CompPeer<Num>::multiply_const(
    string first,
    int64_t second,
    string recombination_key, vertex_t l) {

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  vlm.at(first);
  const auto result = vlm[first] * second;
  vlm.insert( make_pair(recombination_key, result) );

}


template<const size_t Num>
void CompPeer<Num>::multiply_eq(
    string first,
    string recombination_key, vertex_t l) {

  LOG4CXX_TRACE( logger_, "CompPeer<Num>::multiply");

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  vlm.at(first);

  const string key = recombination_key + "_" + lexical_cast<string>(id_);
  const int64_t result = mod(vlm[first] * vlm[first], PRIME_EQ);

  vertex.sig_recombine[recombination_key] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_recombine[recombination_key]) =
      boost::bind(&CompPeer<Num>::recombine, this, recombination_key, l);


  distribute_secret(key, result, l, vertex.clients_[id_]);
}


template<const size_t Num>
void CompPeer<Num>::multiply(
    string first,
    string second,
    string recombination_key, vertex_t l) {

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  vlm.at(first);
  vlm.at(second);

  const string key = recombination_key + "_" + lexical_cast<string>(id_);
  const int64_t result = vlm[first] * vlm[second];

  LOG4CXX_TRACE( logger_, "CompPeer<Num>::multiply " << recombination_key);


  vertex.sig_recombine[recombination_key] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_recombine[recombination_key]) =
      boost::bind(&CompPeer<Num>::recombine, this, recombination_key, l);

  distribute_secret(key, result, l, vertex.clients_[id_]);
}



template<const size_t Num>
void CompPeer<Num>::recombine(string recombination_key, vertex_t l) {

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  gsl_vector* ds = gsl_vector_alloc(3);

  std::stringstream debug_stream;
  for(size_t i = 0; i < Num; i++) {
    const string key = recombination_key + "_" + lexical_cast<string>(i + 1);

    try {
      //vlm.at(key);
    } catch (...) {
      LOG4CXX_FATAL(logger_, "CompPeer<Num>::recombine( " << key << " )");
      //throw std::runtime_error(error);
    }

    const int64_t& value = vlm[key];

    gsl_vector_set(ds, i,  value);
    debug_stream <<  " " << value;
  }

  double recombine;

  gsl_blas_ddot(ds, recombination_vercor_, &recombine);

  vlm[recombination_key] = recombine;
  debug_stream << ": " << recombine;
  LOG4CXX_TRACE(logger_, id_ << ": recombine:" << debug_stream.str());

  gsl_vector_free(ds);

  vertex.sig_bgp_next[recombination_key]->operator ()();
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


template<const size_t Num>
void CompPeer<Num>::distribute(string final_key, int64_t value, vertex_t affected_vertex) {

  Vertex& vertex = bgp_->graph_[affected_vertex];

  vertex.sig_recombine[final_key] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_recombine[final_key]) =
      boost::bind(&CompPeer<3>::interpolate, this, final_key, affected_vertex);

  for(size_t i = 0; i < COMP_PEER_NUM; i++) {
    vertex.clients_[id_][i]->publish(
      final_key + "_" + lexical_cast<string>(id_), value, affected_vertex);
  }

}


template<const size_t Num>
void CompPeer<Num>::interpolate(string final_key, vertex_t l) {

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  double X[Num], Y[Num], D[Num];

  for(size_t i = 0; i < Num; i++) {
    std::string key = final_key + "_"  + boost::lexical_cast<std::string>(i + 1);
    const auto value = vlm[key];
    X[i] = i + 1;
    Y[i] = value;

    LOG4CXX_DEBUG( logger_, "intermediary_: " << key << ": " << value);
  }

  gsl_poly_dd_init( D, X, Y, 3 );
  const double interpol = gsl_poly_dd_eval( D, X, 3, 0);

  const double end = mod(interpol, PRIME);
  LOG4CXX_DEBUG( logger_, "Result: " << ": " << end);
  vlm[final_key] = end;

  vertex.sig_bgp_next[final_key]->operator ()();
}


#endif
