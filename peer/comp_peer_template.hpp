
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

  int& counter = vertex.couter_map_2[rkey];

  value_map_t& vlm = vertex.value_map_;

  LOG4CXX_TRACE(logger_, "Counter... (" << v << "): " << rkey << " " << counter);
  LOG4CXX_TRACE(logger_, " Received value: " << key << ": " << value << " (" << v << ")");


  vlm[key] = value;
  vertex.mutex_->lock();
  counter++;

  if (counter == 3) {
    counter = 0;
    vertex.mutex_->unlock();
    try {
      vertex.sig_recombine[rkey]->operator()();
    } catch (std::exception& e) {
      LOG4CXX_INFO(logger_, " Unable to continue");
      exit(0);
    }
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
  result = mod(result, PRIME);

  const string key = recombination_key + lexical_cast<string>(id_);
  vlm[recombination_key] = result;
}



template<const size_t Num>
void CompPeer<Num>::compare0(string key1, string key2, vertex_t l) {

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  LOG4CXX_DEBUG( logger_, id_ << " (" << l <<  ") " << ": w: " << vlm[w]);
  LOG4CXX_DEBUG( logger_, id_ << " (" << l <<  ") " << ": x: " << vlm[x]);
  LOG4CXX_DEBUG( logger_, id_ << " (" << l <<  ") " << ": y: " << vlm[y]);

  vector<string> circut = {"*", w, x};
  string circut_str = x + "*" + w;

  vertex.sig_compare[circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_compare[circut_str]) =
      boost::bind(&CompPeer<Num>::compare1, this, key1, key2, l);


  vertex.sig_recombine[circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_recombine[circut_str]) =
      boost::bind(&CompPeer<Num>::recombine, this, circut_str, l);

  execute(circut, l);
}



template<const size_t Num>
void CompPeer<Num>::compare1(string key1, string key2, vertex_t l) {

  LOG4CXX_INFO( logger_,  id_ << "compare1");


  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  vector<string> circut = {"*", w, y};
  string circut_str = y + "*" + w;

  string wx = x + "*" + w;
  LOG4CXX_DEBUG( logger_,  id_ << ": wx: " << wx << ": " << vlm[wx]);


  vertex.sig_compare[circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_compare[circut_str]) =
      boost::bind(&CompPeer<Num>::compare2, this, key1, key2, l);


  vertex.sig_recombine[circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_recombine[circut_str]) =
      boost::bind(&CompPeer<Num>::recombine, this, circut_str, l);

  execute(circut, l);
}


template<const size_t Num>
void CompPeer<Num>::compare2(string key1, string key2, vertex_t l) {

  LOG4CXX_INFO( logger_,  id_ << "compare2");

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  vector<string> circut = {"*", x, y};
  string circut_str = y + "*" + x;
  string wy = y + "*" + w;

  LOG4CXX_DEBUG( logger_,  id_ << ": wy: " << wy << ": " << vlm[wy]);


  vertex.sig_compare[circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_compare[circut_str]) =
      boost::bind(&CompPeer<Num>::compare3, this, key1, key2, l);


  vertex.sig_recombine[circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_recombine[circut_str]) =
      boost::bind(&CompPeer<Num>::recombine, this, circut_str, l);

  execute(circut, l);
}



template<const size_t Num>
void CompPeer<Num>::compare3(string key1, string key2, vertex_t l) {

  LOG4CXX_INFO( logger_,  id_ << "compare3");

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  string xy = y + "*" + x;

  vector<string> circut = {"*", w, "*", "2", xy};
  string circut_str = xy + "*" + "2" + "*" + w;


  LOG4CXX_DEBUG( logger_,  id_ << ": xy: " << xy << ": " << vlm[xy]);
  vertex.sig_compare[circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_compare[circut_str]) =
      boost::bind(&CompPeer<Num>::compare4, this, key1, key2, l);


  vertex.sig_recombine[circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_recombine[circut_str]) =
      boost::bind(&CompPeer<Num>::recombine, this, circut_str, l);

  execute(circut, l);
}



template<const size_t Num>
void CompPeer<Num>::compare4(string key1, string key2, vertex_t l) {

  LOG4CXX_INFO( logger_,  id_ << "compare4");

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  string xy = y + "*" + x;
  string wx = x + "*" + w;
  string wy = y + "*" + w;

  string wxy2 = xy + "*" + "2" + "*" + w;

  std::vector<string> circut = {"+", xy, "-", x, "-", y, "-", wxy2, "+", wy, wx};
  string circut_str = wx + "+" + wy + "-" + wxy2 + "-" + y + "-" + x + "+" + xy;

  LOG4CXX_DEBUG( logger_,  id_ << ": 2xyw: " << wxy2 << ": " << vlm[wxy2]);


  vertex.sig_compare[circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_compare[circut_str]) =
      boost::bind(&CompPeer<Num>::compare5, this, key1, key2, l);


  vertex.sig_recombine[circut_str] = shared_ptr< boost::function<void ()> >(
      new boost::function<void ()>
  );

  *(vertex.sig_recombine[circut_str]) =
      boost::bind(&CompPeer<Num>::recombine, this, circut_str, l);

  execute(circut, l);

  LOG4CXX_DEBUG( logger_,  id_ << ": Final: " << ": " << vlm[circut_str] );


  auto value = vlm[circut_str] + 1;
  value = mod(value, PRIME);


  for(size_t i = 0; i < COMP_PEER_NUM; i++) {
    vertex.clients_[id_][i]->publish(circut_str + "_" + lexical_cast<string>(id_), value, l);
  }

}



template<const size_t Num>
void CompPeer<Num>::compare5(string key1, string key2, vertex_t l) {

  LOG4CXX_INFO( logger_,  id_ << "compare5");

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

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
    X[i] = i + 1;
    Y[i] = value;

    LOG4CXX_DEBUG( logger_, "intermediary_: " << key << ": " << value);
  }

  gsl_poly_dd_init( D, X, Y, 3 );
  const double interpol = gsl_poly_dd_eval( D, X, 3, 0);

  const double end = mod(interpol, PRIME);
  LOG4CXX_DEBUG( logger_, "Result: " << ": " << end);


  vertex.sig_bgp_cnt[result]->operator ()(( end ));
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
void CompPeer<Num>::multiply(
    string first,
    string second,
    string recombination_key, vertex_t l) {

  LOG4CXX_TRACE( logger_, "CompPeer<Num>::multiply");

  Vertex& vertex = bgp_->graph_[l];
  value_map_t& vlm = vertex.value_map_;

  vlm.at(first);
  vlm.at(second);

  const string key = recombination_key + "_" + lexical_cast<string>(id_);
  const int64_t result = vlm[first] * vlm[second];

  distribute_secret(key, result, l, vertex.clients_[id_]);
}



template<const size_t Num>
void CompPeer<Num>::recombine(string recombination_key, vertex_t l) {

  LOG4CXX_INFO( logger_, "CompPeer<Num>::recombine -> " << l);

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


  vertex.sig_compare[recombination_key]->operator ()();
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
