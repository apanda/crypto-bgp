#include <bgp/bgp.hpp>
#include <bgp/common.hpp>
#include <bgp/vertex.hpp>
#include <bgp/edge.hpp>
#include <peer/comp_peer.hpp>

#include <algorithm>

BGPProcess::BGPProcess(
    string path,
    CompPeer<3> * comp_peer,
    io_service& io):
    comp_peer_(comp_peer),
    io_service_(io) {

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
    vertex.set_preference();
  }

}



void BGPProcess::start(graph_t& graph) {

  vertex_t dst_vertex = 0;
  Vertex& dst = graph[dst_vertex];

  shared_ptr< set<vertex_t> > affected_ptr(new set<vertex_t>);
  shared_ptr< tbb::concurrent_unordered_set<vertex_t> > changed_ptr(
      new tbb::concurrent_unordered_set<vertex_t>);

  set<vertex_t>& affected = *(affected_ptr);
  tbb::concurrent_unordered_set<vertex_t>& changed = *changed_ptr;

  changed.insert(dst_vertex);
  for(const vertex_t& vertex: dst.neigh_) {
    affected.insert(vertex);
  }

  next_iteration_start(dst_vertex, affected_ptr, changed_ptr);
}



void BGPProcess::next_iteration_start(
    const vertex_t dst_vertex,
    shared_ptr< set<vertex_t> > affected_set_ptr,
    shared_ptr< tbb::concurrent_unordered_set<vertex_t> > changed_set_ptr) {

  set<vertex_t>& affected_set = *(affected_set_ptr);
  tbb::concurrent_unordered_set<vertex_t>& changed_set = *changed_set_ptr;

  LOG4CXX_INFO(comp_peer_->logger_,
      "Next iteration... " << affected_set.size() << ": " << changed_set.size());

  shared_ptr<tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr (
      new tbb::concurrent_unordered_set<vertex_t>);

  shared_ptr < vector<vertex_t> > batch_ptr(new vector<vertex_t>);
  vector<vertex_t>& batch = *batch_ptr;

  for(const auto vertex: affected_set) {
    batch.push_back(vertex);
  }

  continuation_ = boost::bind(
        &BGPProcess::next_iteration_continue,
        this, dst_vertex, batch_ptr, affected_set_ptr,
        changed_set_ptr, new_changed_set_ptr);

  continuation_();
}


void BGPProcess::next_iteration_continue(
    const vertex_t dst_vertex,
    shared_ptr< vector<vertex_t> > batch_ptr,
    shared_ptr< set<vertex_t> > affected_set_ptr,
    shared_ptr< tbb::concurrent_unordered_set<vertex_t> > changed_set_ptr,
    shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr) {

  vector<vertex_t>& batch = *batch_ptr;

  shared_ptr< pair<size_t, size_t> > counts_ptr(new pair<size_t, size_t>);
  size_t& count = counts_ptr->first;
  count = 0;

  vector<vertex_t> current_batch;

  for(;;) {
    if (batch.empty()) break;
    if (current_batch.size() == TASK_COUNT) break;

    const vertex_t vertex = batch.back();
    batch.pop_back();

    if (vertex < VERTEX_START) continue;
    if (vertex > VERTEX_END) continue;

    if (vertex == dst_vertex) continue;
    current_batch.push_back(vertex);
  }

  if (current_batch.empty()) {
    next_iteration_finish(dst_vertex, new_changed_set_ptr);
    return;
  }

  counts_ptr->second = current_batch.size();


  for(auto& vertex: current_batch) {
    io_service_.post(
      boost::bind(
          &BGPProcess::process_neighbors_mpc,
          this, vertex,
          changed_set_ptr,
          new_changed_set_ptr,
          counts_ptr)
    );
  }

}


void BGPProcess::next_iteration_finish(
    const vertex_t dst_vertex,
    shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr) {

  tbb::concurrent_unordered_set<vertex_t>& new_changed_set = *new_changed_set_ptr;

  vector<vertex_t> nodes;

  for(const vertex_t vertex: new_changed_set) {
    nodes.push_back(vertex);
  }
  new_changed_set.clear();

  master_->sync(nodes);
  master_->barrier_->wait();

  LOG4CXX_INFO(comp_peer_->logger_,
      "Syncing up with the master... size: " << master_->size_);

  for(size_t i = 0; i < master_->size_; i++) {
    new_changed_set.insert(master_->array_[i]);
  }

  shared_ptr<  set<vertex_t> > new_affected_set_ptr(new set<vertex_t>);
  set<vertex_t>& new_affected_set = *new_affected_set_ptr;

  for(const vertex_t vertex: new_changed_set) {
    auto neighbors = adjacent_vertices(vertex, graph_);
    new_affected_set.insert(neighbors.first, neighbors.second);
  }

  if(new_changed_set.empty())  {
    end_();
    return;
  }

  next_iteration_start(dst_vertex, new_affected_set_ptr, new_changed_set_ptr);
}



void BGPProcess::process_neighbors_mpc(
    const vertex_t affected_vertex,
    shared_ptr< tbb::concurrent_unordered_set<vertex_t> > changed_set_ptr,
    shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr< pair<size_t, size_t> > counts_ptr) {

  tbb::concurrent_unordered_set<vertex_t>& changed_set = *changed_set_ptr;

  shared_ptr< vector<vertex_t> > intersection_ptr(new vector<vertex_t>);
  vector<vertex_t>& intersection = *intersection_ptr;

  Vertex& affected = graph_[affected_vertex];
  auto& neighs = affected.neigh_;

  auto ch = vector<vertex_t>( changed_set.begin() , changed_set.end() );
  std::sort(ch.begin(), ch.end());

  std::set_intersection( neighs.begin(), neighs.end(), ch.begin(), ch.end(),
      std::insert_iterator< std::vector<vertex_t> >( intersection, intersection.begin() ) );

  //LOG4CXX_INFO(comp_peer_->logger_, "Intersection size for vertex "
  //    << affected.id_ << ": " << intersection.size() << ": " << neighs.size());

  if (intersection.size() < 200) {
    for0(affected_vertex, new_changed_set_ptr, counts_ptr, intersection_ptr);
  } else {

    shared_ptr< pair<size_t, size_t> > suncounter_ptr(new pair<size_t, size_t>);
    suncounter_ptr->second = (intersection.size() + (MAX_BATCH - 1)) / MAX_BATCH;

    size_t offset = 0;
    while (offset < intersection.size()) {
      vector<vertex_t>::iterator start = intersection.begin() + offset;
      vector<vertex_t>::iterator end = intersection.begin() + offset + MAX_BATCH;
      offset += MAX_BATCH;

      auto pair = std::make_pair(start, end);
      compute_partial0(
          affected_vertex, new_changed_set_ptr,
          counts_ptr, suncounter_ptr, intersection_ptr, pair);
    }
  }

}



void BGPProcess::compute_partial0(
    const vertex_t affected_vertex,
    shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr< pair<size_t, size_t> > counts_ptr,
    shared_ptr< pair<size_t, size_t> > subcounter_ptr,
    shared_ptr< vector<vertex_t> > n_ptr,
    pair<vector<vertex_t>::iterator, vector<vertex_t>::iterator> iters) {

  vector<vertex_t>& n = *n_ptr;
  size_t& count = counts_ptr->first;
  size_t& batch_count = counts_ptr->second;

  const bool is_end =  (iters.first == iters.second) || (iters.first == n_ptr->end());

  if (is_end) {
    m_.lock();
    subcounter_ptr->first++;

    if (subcounter_ptr->first == subcounter_ptr->second) {
        count++;
      if (batch_count == count) {
        m_.unlock();
        continuation_();
        return;
      }
    }

    m_.unlock();
    return;
  }

  Vertex& affected = graph_[affected_vertex];
  const vertex_t neigh_vertex = *(iters.first);
  iters.first++;

  const string key1 = lexical_cast<string>(affected.next_hop_);
  const string key2 = lexical_cast<string>(neigh_vertex);

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  string xy = y + "*" + x;
  string wx = x + "*" + w;
  string wy = y + "*" + w;

  string wxy2 = xy + "*" + "2" + "*" + w;
  string result = wx + "+" + wy + "-" + wxy2 + "-" + y + "-" + x + "+" + xy;

  affected.sig_bgp_next[result] =
      shared_ptr<boost::function<void()> >(new boost::function<void()>);

  auto next = boost::bind(&BGPProcess::compute_partial0, this,
        affected_vertex,
        new_changed_set_ptr,
        counts_ptr,
        subcounter_ptr,
        n_ptr,
        iters
      );

  *(affected.sig_bgp_next[result]) = next;

  LOG4CXX_DEBUG(comp_peer_->logger_, "Vertex -> "
      << affected_vertex << ", " << neigh_vertex );

  affected.sig_bgp_cnt[result] = shared_ptr<boost::function<void(int)> >(new boost::function<void(int)>);

  *(affected.sig_bgp_cnt[result]) = boost::bind(&BGPProcess::for1, this,
              affected_vertex,
              neigh_vertex,
              new_changed_set_ptr,
              _1);

  comp_peer_->compare0(
      lexical_cast<string>(affected.next_hop_),
      lexical_cast<string>(neigh_vertex),
      affected_vertex);
}




void BGPProcess::for0(
    const vertex_t affected_vertex,
    shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    shared_ptr< pair<size_t, size_t> > counts_ptr,
    shared_ptr< vector<vertex_t> > n_ptr) {

  vector<vertex_t>& n = *n_ptr;
  size_t& count = counts_ptr->first;
  size_t& batch_count = counts_ptr->second;

  if (n.empty()) {
    {
    boost::unique_lock<boost::mutex> lock(m_);
    count++;
    }
    if (batch_count == count) {
      continuation_();
    }
    return;
  }

  Vertex& affected = graph_[affected_vertex];
  const vertex_t neigh_vertex = n.back();
  n.pop_back();

  const string key1 = lexical_cast<string>(affected.next_hop_);
  const string key2 = lexical_cast<string>(neigh_vertex);

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  string xy = y + "*" + x;
  string wx = x + "*" + w;
  string wy = y + "*" + w;

  string wxy2 = xy + "*" + "2" + "*" + w;
  string result = wx + "+" + wy + "-" + wxy2 + "-" + y + "-" + x + "+" + xy;

  affected.sig_bgp_next[result] =
      shared_ptr<boost::function<void()> >(new boost::function<void()>);

  auto next = boost::bind(&BGPProcess::for0, this,
        affected_vertex,
        new_changed_set_ptr,
        counts_ptr,
        n_ptr
      );

  *(affected.sig_bgp_next[result]) = next;

  LOG4CXX_DEBUG(comp_peer_->logger_, "Vertex -> "
      << affected_vertex << ", " << neigh_vertex );

  affected.sig_bgp_cnt[result] = shared_ptr<boost::function<void(int)> >(new boost::function<void(int)>);

  *(affected.sig_bgp_cnt[result]) = boost::bind(&BGPProcess::for1, this,
              affected_vertex,
              neigh_vertex,
              new_changed_set_ptr,
              _1);

  comp_peer_->compare0(
      lexical_cast<string>(affected.next_hop_),
      lexical_cast<string>(neigh_vertex),
      affected_vertex);
}



void BGPProcess::compute_partial1(
    vertex_t affected_vertex,
    vertex_t neigh_vertex,
    shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    int cmp) {

  tbb::concurrent_unordered_set<vertex_t>& new_changed_set = *new_changed_set_ptr;
  Vertex& affected = graph_[affected_vertex];

  const string key1 = lexical_cast<string>(affected.next_hop_);
  const string key2 = lexical_cast<string>(neigh_vertex);

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  string xy = y + "*" + x;
  string wx = x + "*" + w;
  string wy = y + "*" + w;

  string wxy2 = xy + "*" + "2" + "*" + w;
  string result = wx + "+" + wy + "-" + wxy2 + "-" + y + "-" + x + "+" + xy;

  const auto current_preference = affected.current_next_hop_preference(graph_);

  auto offer_it = affected.preference_.find(neigh_vertex);
  BOOST_ASSERT(offer_it != affected.preference_.end());
  const auto offered_preference = offer_it->second;

  LOG4CXX_DEBUG(comp_peer_->logger_, "Compare -> "
      << current_preference << ", " << offered_preference );



  const bool condition = offered_preference <= current_preference;

  if (cmp != condition) {

    LOG4CXX_FATAL(comp_peer_->logger_, "==================================================");
    LOG4CXX_FATAL(comp_peer_->logger_,
        "(Is, Should): " <<
        "(" << cmp << ", " << condition << ") -- " <<
        "(" << affected_vertex << ", " << neigh_vertex << ")");
    LOG4CXX_FATAL(comp_peer_->logger_, "==================================================");

  }

  if ( offered_preference <= current_preference ) {
    affected.sig_bgp_next[result]->operator()();
    return;
  }

  affected.set_next_hop(graph_, neigh_vertex);
  new_changed_set.insert(affected_vertex);

  affected.sig_bgp_next[result]->operator ()();
}



void BGPProcess::for1(
    vertex_t affected_vertex,
    vertex_t neigh_vertex,
    shared_ptr< tbb::concurrent_unordered_set<vertex_t> > new_changed_set_ptr,
    int cmp) {

  tbb::concurrent_unordered_set<vertex_t>& new_changed_set = *new_changed_set_ptr;
  Vertex& affected = graph_[affected_vertex];

  const string key1 = lexical_cast<string>(affected.next_hop_);
  const string key2 = lexical_cast<string>(neigh_vertex);

  string w = ".2" + key1;
  string x = ".2" + key2;
  string y = ".2" + key1 + "-" + key2;

  string xy = y + "*" + x;
  string wx = x + "*" + w;
  string wy = y + "*" + w;

  string wxy2 = xy + "*" + "2" + "*" + w;
  string result = wx + "+" + wy + "-" + wxy2 + "-" + y + "-" + x + "+" + xy;

  const auto current_preference = affected.current_next_hop_preference(graph_);

  auto offer_it = affected.preference_.find(neigh_vertex);
  BOOST_ASSERT(offer_it != affected.preference_.end());
  const auto offered_preference = offer_it->second;

  LOG4CXX_DEBUG(comp_peer_->logger_, "Compare -> "
      << current_preference << ", " << offered_preference );



  const bool condition = offered_preference <= current_preference;

  if (cmp != condition) {

    LOG4CXX_FATAL(comp_peer_->logger_, "==================================================");
    LOG4CXX_FATAL(comp_peer_->logger_,
        "(Is, Should): " <<
        "(" << cmp << ", " << condition << ") -- " <<
        "(" << affected_vertex << ", " << neigh_vertex << ")");
    LOG4CXX_FATAL(comp_peer_->logger_, "==================================================");

  }

  if ( offered_preference <= current_preference ) {
    affected.sig_bgp_next[result]->operator()();
    return;
  }

  affected.set_next_hop(graph_, neigh_vertex);
  new_changed_set.insert(affected_vertex);

  affected.sig_bgp_next[result]->operator ()();
}



void BGPProcess::load_graph(string path, graph_t& graph) {

  dynamic_properties dp;
  std::ifstream file(path);

  dp.property("node_id", get(&Vertex::id_, graph));
  dp.property("key", get(&Edge::key_, graph));

  read_graphviz(file ,graph, dp, "node_id");
}



void BGPProcess::print_state(graph_t& graph) {
  auto iter = vertices(graph);
  auto last = iter.second;
  auto current = iter.first;

  for (; current != last; ++current) {
    const auto& current_vertex = *current;
    Vertex& vertex = graph[current_vertex];
    std::cout << vertex;
  }
}



void BGPProcess::print_state(
    graph_t& graph,
    set<vertex_t>& affected_set,
    set<vertex_t>& changed_set) {

  std::cout << "Changed: ";
  for(auto a: affected_set) {
    std::cout << a << " ";
  }

  std::cout << std::endl;;

  std::cout << "Affected: ";
  for(auto a: changed_set) {
    std::cout << a << " ";
  }

  std::cout << std::endl;
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
