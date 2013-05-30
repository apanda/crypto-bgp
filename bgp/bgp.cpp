#include <bgp/bgp.hpp>
#include <bgp/common.hpp>
#include <bgp/vertex.hpp>
#include <bgp/edge.hpp>
#include <peer/comp_peer.hpp>
#include <deque>

#include <algorithm>

BGPProcess::BGPProcess(string path, CompPeer<3> * comp_peer, io_service& io) :
    graph_(GRAPH_SIZE), comp_peer_(comp_peer), io_service_(io) {

  load_graph(path, graph_);
  init(graph_);

}

void BGPProcess::start_callback(function<bool()> f) {

  start(graph_);
  end_ = f;

}

void BGPProcess::init(graph_t& graph) {

  auto iter = vertices(graph);
  auto last = iter.second;
  auto current = iter.first;

  for (; current != last; ++current) {
    const auto& current_vertex = *current;
    Vertex& vertex = graph[current_vertex];
    vertex.id_ = current_vertex;

    vertex.set_neighbors(graph);
    //vertex.set_preference();
  }

}

void BGPProcess::start(graph_t& graph) {

  vertex_t dst_vertex = 1;
  Vertex& dst = graph[dst_vertex];
  dst.next_hop_ = dst_vertex;

  shared_ptr<set<vertex_t> > affected_ptr(new set<vertex_t>);
  shared_ptr<tbb::concurrent_unordered_set<vertex_t> > changed_ptr(
      new tbb::concurrent_unordered_set<vertex_t>);

  tbb::concurrent_unordered_set<vertex_t>& changed = *changed_ptr;

  changed.insert(dst_vertex);
  for (const vertex_t& vertex : dst.neigh_) {
    changed.insert(vertex);
  }

  next_iteration_start(dst_vertex, affected_ptr, changed_ptr);
}



void BGPProcess::next_iteration_start(const vertex_t dst_vertex,
    shared_ptr<set<vertex_t> > affected_set_ptr,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > changed_set_ptr) {

  set<vertex_t>& affected_set = *(affected_set_ptr);
  tbb::concurrent_unordered_set<vertex_t>& changed_set = *changed_set_ptr;

  affected_set.insert(dst_vertex);

  LOG4CXX_INFO(comp_peer_->logger_,
      "Next iteration... " << affected_set.size() << ": " << changed_set.size());

  shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr(
      new tbb::concurrent_unordered_set<vertex_t>);

  shared_ptr<vector<vertex_t> > batch_ptr(new vector<vertex_t>);
  vector<vertex_t>& batch = *batch_ptr;


  for (const auto vertex : affected_set) {
    batch.push_back(vertex);
  }

  if (batch.empty()) {
    next_iteration_finish(dst_vertex, new_changed_set_ptr);
  } else {
    next_iteration_continue(dst_vertex, batch_ptr, affected_set_ptr,
        changed_set_ptr, new_changed_set_ptr);
  }
}



void BGPProcess::next_iteration_continue(const vertex_t dst_vertex,
    shared_ptr<vector<vertex_t> > batch_ptr,
    shared_ptr<set<vertex_t> > affected_set_ptr,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > changed_set_ptr,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr) {

  continuation_ = boost::bind(&BGPProcess::next_iteration_finish, this,
      dst_vertex, new_changed_set_ptr);

  vector<vertex_t>& batch = *batch_ptr;

  shared_ptr<pair<size_t, size_t> > counts_ptr(new pair<size_t, size_t>);
  counts_ptr->first = 0;
  counts_ptr->second = batch.size();

  for (auto& vertex : batch) {

    io_service_.post(
        boost::bind(&BGPProcess::process_neighbors_mpc, this, vertex,
            changed_set_ptr, new_changed_set_ptr, counts_ptr));
  }

}



void BGPProcess::next_iteration_finish(const vertex_t dst_vertex,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr) {

  end_();
}



const string BGPProcess::get_recombination(vector<string>& circut) {
  return circut[2] + circut[0] + circut[1];
}



void BGPProcess::process_neighbors_mpc(const vertex_t affected_vertex,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > changed_set_ptr,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr<pair<size_t, size_t> > counts_ptr) {

  tbb::concurrent_unordered_set<vertex_t>& changed_set = *changed_set_ptr;

  shared_ptr<vector<vertex_t> > intersection_ptr(new vector<vertex_t>);
  vector<vertex_t>& intersection = *intersection_ptr;

  Vertex& affected = graph_[affected_vertex];
  auto& vlm = affected.value_map_;
  auto& neighs = affected.neigh_;

  auto ch = vector<vertex_t>(changed_set.begin(), changed_set.end());
  std::sort(ch.begin(), ch.end());

  shared_ptr<deque<pref_pair_t> > prefs_ptr(new deque<pref_pair_t>);
  std::deque<pref_pair_t>& prefs = *prefs_ptr;

  std::set_intersection(neighs.begin(), neighs.end(), ch.begin(), ch.end(),
      std::insert_iterator<std::vector<vertex_t> >(intersection,
          intersection.begin()));

  size_t counter = 1;
  for (auto& neigh : intersection) {
    const auto pref = counter;
    const auto pref_pair = std::make_pair(neigh, pref);
    prefs.push_back(pref_pair);

    counter++;
    if(counter % 3 == 0) counter++;
  }

  std::sort(prefs.begin(), prefs.end(),
      boost::bind(&pref_pair_t::second, _1)
          > boost::bind(&pref_pair_t::second, _2));

  vlm["result"] = 0;
  vlm["acc0"] = 1;
  vlm["eql0"] = 1;
  vlm["neq0"] = 0;

  shared_ptr<size_t> local_counts_ptr(new size_t);
  size_t& local_count = *local_counts_ptr;
  local_count = 0;

  for0(
      affected_vertex, new_changed_set_ptr,
      counts_ptr, local_counts_ptr, prefs_ptr);
}



void BGPProcess::for0(const vertex_t affected_vertex,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr<pair<size_t, size_t> > counts_ptr,
    shared_ptr<size_t> local_counts_ptr,
    shared_ptr<deque<pref_pair_t> > prefs_ptr) {

  deque<pref_pair_t>& prefs = *prefs_ptr;

  if (prefs.empty()) {
    for_distribute(affected_vertex, new_changed_set_ptr, counts_ptr,
        local_counts_ptr, prefs_ptr);
    return;
  }

  size_t& local_count = *local_counts_ptr;
  local_count++;

  const auto pref = prefs.front();
  prefs.pop_front();

  LOG4CXX_DEBUG(comp_peer_->logger_,
      "Preference: " << affected_vertex << " | " << pref.first << " | " << pref.second);

  Vertex& affected = graph_[affected_vertex];
  auto& vlm = affected.value_map_;

  const vertex_t offered_vertex = pref.first;
  Vertex& offered = graph_[offered_vertex];

  const string key = lexical_cast<string>(local_count);
  const string prev_key = lexical_cast<string>(local_count - 1);

  string pol_key = "pol" + key;
  string val_key = "val" + key;
  string eql_key = "eql" + key;

  vector<string> circut;
  circut = {"==", "0", pol_key};
  string for0_key = get_recombination(circut);

  string final_key = get_recombination(circut);

  affected.sig_bgp_next[final_key] = shared_ptr<boost::function<void()> >(
      new boost::function<void()>);

  *(affected.sig_bgp_next[final_key]) = boost::bind(&BGPProcess::for1, this,
      affected_vertex, new_changed_set_ptr, counts_ptr, local_counts_ptr,
      prefs_ptr);

  vlm[val_key] = pref.first;
  vlm[pol_key] = offered.get_export(affected_vertex);

  comp_peer_->execute(circut, affected_vertex);
}



void BGPProcess::for1(const vertex_t affected_vertex,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr<pair<size_t, size_t> > counts_ptr,
    shared_ptr<size_t> local_counts_ptr,
    shared_ptr<deque<pref_pair_t> > prefs_ptr) {

  size_t& local_count = *local_counts_ptr;

  Vertex& affected = graph_[affected_vertex];
  auto& vlm = affected.value_map_;

  const string key = lexical_cast<string>(local_count);
  const string prev_key = lexical_cast<string>(local_count - 1);

  string pol_key = "pol" + key;
  string val_key = "val" + key;
  string eql_key = "eql" + key;
  string neq_key = "neq" + key;

  vector<string> circut;
  circut = {"==", "0", pol_key};
  string for0_key = get_recombination(circut);

  string pre_eql_key = "eql" + prev_key;
  string pre_acc_key = "acc" + prev_key;
  circut = {"*", pre_eql_key, pre_acc_key};

  string for1_key = get_recombination(circut);
  string final_key = for1_key;

  affected.sig_bgp_next[final_key] = shared_ptr<boost::function<void()> >(
      new boost::function<void()>);

  *(affected.sig_bgp_next[final_key]) = boost::bind(&BGPProcess::for2, this,
      affected_vertex, new_changed_set_ptr, counts_ptr, local_counts_ptr,
      prefs_ptr);

  vlm[eql_key] = 1 - vlm[for0_key];
  vlm[neq_key] = vlm[for0_key];

  //LOG4CXX_INFO(comp_peer_->logger_, "for0: " << affected_vertex << " | " << vlm[for0_key]);

  comp_peer_->execute(circut, affected_vertex);

}



void BGPProcess::for2(const vertex_t affected_vertex,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr<pair<size_t, size_t> > counts_ptr,
    shared_ptr<size_t> local_counts_ptr,
    shared_ptr<deque<pref_pair_t> > prefs_ptr) {

  size_t& local_count = *local_counts_ptr;

  Vertex& affected = graph_[affected_vertex];
  auto& vlm = affected.value_map_;

  const string key = lexical_cast<string>(local_count);
  const string prev_key = lexical_cast<string>(local_count - 1);

  string pol_key = "pol" + key;
  string eql_key = "eql" + key;
  string neq_key = "neq" + key;

  vector<string> circut;
  circut = {"==", "0", pol_key};
  string for0_key = get_recombination(circut);

  string pre_eql_key = "eql" + prev_key;
  string pre_acc_key = "acc" + prev_key;
  circut = {"*", pre_eql_key, pre_acc_key};
  string for1_key = get_recombination(circut);

  string acc_key = "acc" + key;
  circut = {"*", neq_key, acc_key};
  string for2_key = get_recombination(circut);
  string final_key = for2_key;

  affected.sig_bgp_next[final_key] = shared_ptr<boost::function<void()> >(
      new boost::function<void()>);

  *(affected.sig_bgp_next[final_key]) = boost::bind(&BGPProcess::for3, this,
      affected_vertex, new_changed_set_ptr, counts_ptr, local_counts_ptr,
      prefs_ptr);

  vlm[acc_key] = vlm[for1_key];

  //LOG4CXX_INFO(comp_peer_->logger_, "for1: " << affected_vertex << " | " << vlm[for1_key]);

  comp_peer_->execute(circut, affected_vertex);
}



void BGPProcess::for3(const vertex_t affected_vertex,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr<pair<size_t, size_t> > counts_ptr,
    shared_ptr<size_t> local_counts_ptr,
    shared_ptr<deque<pref_pair_t> > prefs_ptr) {

  size_t& local_count = *local_counts_ptr;

  Vertex& affected = graph_[affected_vertex];
  //auto& vlm = affected.value_map_;

  const string key = lexical_cast<string>(local_count);
  const string prev_key = lexical_cast<string>(local_count - 1);

  string eql_key = "eql" + key;
  string neq_key = "neq" + key;

  vector<string> circut;
  circut = {"==", "0", eql_key};
  string for0_key = get_recombination(circut);

  string pre_eql_key = "eql" + prev_key;
  string pre_acc_key = "acc" + prev_key;
  circut = {"*", pre_eql_key, pre_acc_key};
  string for1_key = get_recombination(circut);

  string acc_key = "acc" + key;
  circut = {"*", neq_key, acc_key};
  string for2_key = get_recombination(circut);

  string val_key = "val" + key;
  circut = {"*", for2_key, val_key};
  string for3_key = get_recombination(circut);

  string final_key = get_recombination(circut);

  affected.sig_bgp_next[final_key] = shared_ptr<boost::function<void()> >(
      new boost::function<void()>);

  *(affected.sig_bgp_next[final_key]) = boost::bind(&BGPProcess::for_add, this,
      affected_vertex, new_changed_set_ptr, counts_ptr, local_counts_ptr,
      prefs_ptr);

  //LOG4CXX_INFO(comp_peer_->logger_, "for2: " << affected_vertex << " | " << vlm[for2_key]);

  comp_peer_->execute(circut, affected_vertex);
}



void BGPProcess::for_add(const vertex_t affected_vertex,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr<pair<size_t, size_t> > counts_ptr,
    shared_ptr<size_t> local_counts_ptr,
    shared_ptr<deque<pref_pair_t> > prefs_ptr) {

  size_t& local_count = *local_counts_ptr;

  Vertex& affected = graph_[affected_vertex];
  auto& vlm = affected.value_map_;

  const string key = lexical_cast<string>(local_count);
  const string prev_key = lexical_cast<string>(local_count - 1);

  string eql_key = "eql" + key;
  string neq_key = "neq" + key;

  vector<string> circut;
  circut = {"==", "0", eql_key};
  string for0_key = get_recombination(circut);

  string pre_eql_key = "eql" + prev_key;
  string pre_acc_key = "acc" + prev_key;
  circut = {"*", pre_eql_key, pre_acc_key};
  string for1_key = get_recombination(circut);

  string acc_key = "acc" + key;
  circut = {"*", neq_key, acc_key};
  string for2_key = get_recombination(circut);

  string val_key = "val" + key;
  circut = {"*", for2_key, val_key};
  string for3_key = get_recombination(circut);

  string final_key = get_recombination(circut);

  //LOG4CXX_INFO(comp_peer_->logger_, "for3: " << affected_vertex << " | " << vlm[for3_key]);

  vlm["result"] = vlm["result"] + vlm[final_key];
  for0(affected_vertex, new_changed_set_ptr, counts_ptr, local_counts_ptr,
      prefs_ptr);
}



void BGPProcess::for_distribute(const vertex_t affected_vertex,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr<pair<size_t, size_t> > counts_ptr,
    shared_ptr<size_t> local_counts_ptr,
    shared_ptr<deque<pref_pair_t> > prefs_ptr) {

  size_t& local_count = *local_counts_ptr;

  Vertex& affected = graph_[affected_vertex];
  auto& vlm = affected.value_map_;

  const string key = lexical_cast<string>(local_count);
  const string prev_key = lexical_cast<string>(local_count - 1);

  string eql_key = "eql" + key;
  string neq_key = "neq" + key;

  vector<string> circut;
  circut = {"==", "0", eql_key};
  string for0_key = get_recombination(circut);

  string pre_eql_key = "eql" + prev_key;
  string pre_acc_key = "acc" + prev_key;
  circut = {"*", pre_eql_key, pre_acc_key};
  string for1_key = get_recombination(circut);

  string acc_key = "acc" + key;
  circut = {"*", neq_key, acc_key};
  string for2_key = get_recombination(circut);

  string val_key = "val" + key;
  circut = {"*", for2_key, val_key};
  string for3_key = get_recombination(circut);

  string final_key = get_recombination(circut);

  string result_string = "result";
  const auto value = vlm[result_string];

  affected.sig_bgp_next[result_string] = shared_ptr<boost::function<void()> >(
      new boost::function<void()>);

  *(affected.sig_bgp_next[result_string]) = boost::bind(&BGPProcess::for_final,
      this, affected_vertex, new_changed_set_ptr, counts_ptr, local_counts_ptr,
      prefs_ptr);

  comp_peer_->distribute(result_string, value, affected_vertex);
}



void BGPProcess::for_final(const vertex_t affected_vertex,
    shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr<pair<size_t, size_t> > counts_ptr,
    shared_ptr<size_t> local_counts_ptr,
    shared_ptr<deque<pref_pair_t> > prefs_ptr) {

  size_t& count = counts_ptr->first;
  size_t& all_count = counts_ptr->second;

  Vertex& affected = graph_[affected_vertex];
  auto& vlm = affected.value_map_;

  const string key = lexical_cast<string>(count);
  const string prev_key = lexical_cast<string>(count - 1);

  string result_string = "end";
  const auto value = vlm[result_string];

  LOG4CXX_INFO(comp_peer_->logger_,
      "Result -> " << affected_vertex << " | " << value);

  if (value != affected.next_hop_ && value != 0) {
    affected.next_hop_ = value;
    auto& new_changed_set = *new_changed_set_ptr;
    new_changed_set.insert(affected_vertex);
  }

  affected.couter_map_2.clear();

  m_.lock();
  count++;

  if (count == all_count) {
    m_.unlock();
    continuation_();
  }
  m_.unlock();

}


#include <boost/algorithm/string.hpp>

void BGPProcess::load_graph(string path, graph_t& graph) {

  std::cout << "GPProcess::load_graph " << GRAPH_SIZE << std::endl;>;

  vertex_t dst = 1;

  for(auto i = 2; i < GRAPH_SIZE; i++) {
    vertex_t src = i;
    boost::add_edge(src, dst, graph);
  }

}


size_t BGPProcess::get_graph_size(string path) {
  return 1024 + 2;
}



void BGPProcess::print_result() {

  auto iter = vertices(graph_);
  auto last = iter.second;
  auto current = iter.first;

  LOG4CXX_FATAL(comp_peer_->logger_, "digraph G {");

  for (; current != last; ++current) {
    const auto& current_vertex = *current;
    Vertex& vertex = graph_[current_vertex];
    LOG4CXX_FATAL(comp_peer_->logger_, vertex.id_ << " -> " << vertex.next_hop_);
  }

  LOG4CXX_FATAL(comp_peer_->logger_, "}");

}
