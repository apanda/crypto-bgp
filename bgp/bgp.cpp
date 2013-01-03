#include <bgp/bgp.hpp>
#include <peer/comp_peer.hpp>

#include <bgp/common.hpp>
#include <bgp/vertex.hpp>
#include <bgp/edge.hpp>


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
  f();

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

  set<vertex_t> affected;
  tbb::concurrent_unordered_set<vertex_t> changed;

  changed.insert(dst_vertex);
  for(const vertex_t& vertex: dst.neigh_) {
    affected.insert(vertex);
  }

  next_iteration(dst_vertex, graph, affected, changed);
}



void BGPProcess::next_iteration(
    const vertex_t dst_vertex,
    graph_t& graph,
    set<vertex_t>& affected_set,
    tbb::concurrent_unordered_set<vertex_t> changed_set) {

  printf("Next iteration... %lu: %lu\n", affected_set.size(), changed_set.size());

  boost::thread_group tg;

  set<vertex_t> new_affected_set;
  tbb::concurrent_unordered_set<vertex_t> new_changed_set;

  vector<vertex_t> batch;
  for(const auto vertex: affected_set) {
    batch.push_back(vertex);
  }

  for(;;) {

    size_t count = 0;
    vector<vertex_t> current_batch;

    for(int i = 0; i < TASK_COUNT; i++) {

      if (batch.empty()) break;

      const vertex_t vertex = batch.back();
      batch.pop_back();

      if (vertex == dst_vertex) continue;
      current_batch.push_back(vertex);

      io_service_.post(
        boost::bind(
            &BGPProcess::process_neighbors_mpc,
            this, vertex,
            boost::ref(changed_set),
            boost::ref(new_changed_set),
            boost::ref(count))
      );
    }

    boost::unique_lock<boost::mutex> lock(m_);
    while(count != current_batch.size()) {
      cv_.wait(lock);
    }

    if (batch.empty()) break;

  }

  for(const vertex_t vertex: new_changed_set) {
    auto neighbors = adjacent_vertices(vertex, graph_);
    new_affected_set.insert(neighbors.first, neighbors.second);
  }

  if(new_changed_set.empty()) return;

  next_iteration(dst_vertex, graph, new_affected_set, new_changed_set);
}


#include <algorithm>


void BGPProcess::process_neighbors_mpc(
    const vertex_t affected_vertex,
    tbb::concurrent_unordered_set<vertex_t>& changed_set,
    tbb::concurrent_unordered_set<vertex_t>& new_changed_set,
    size_t& count) {

  vector<vertex_t> intersection;
  Vertex& affected = graph_[affected_vertex];
  auto& neighs = affected.neigh_;

  auto ch = vector<vertex_t>( changed_set.begin() , changed_set.end() );
  std::sort(ch.begin(), ch.end());

  std::set_intersection( neighs.begin(), neighs.end(), ch.begin(), ch.end(),
      std::insert_iterator< std::vector<vertex_t> >( intersection, intersection.begin() ) );

  for0(affected_vertex, new_changed_set, count, intersection);

}




void BGPProcess::for0(
    const vertex_t affected_vertex,
    tbb::concurrent_unordered_set<vertex_t>& new_changed_set,
    size_t& count,
    vector<vertex_t>& n) {


  if (n.empty()) {
    boost::unique_lock<boost::mutex> lock(m_);
    count++;
    cv_.notify_all();
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
        boost::ref(new_changed_set),
        boost::ref(count),
        n
      );

  *(affected.sig_bgp_next[result]) = next;

  LOG4CXX_INFO(comp_peer_->logger_, "Vertex -> "
      << affected_vertex << ", " << neigh_vertex );

  affected.sig_bgp_cnt[result] = shared_ptr<boost::function<void(int)> >(new boost::function<void(int)>);

  *(affected.sig_bgp_cnt[result]) = boost::bind(&BGPProcess::for1, this,
              affected_vertex,
              neigh_vertex,
              boost::ref(new_changed_set),
              _1);

  LOG4CXX_INFO(comp_peer_->logger_, "Compare0")

  comp_peer_->compare0(
      lexical_cast<string>(affected.next_hop_),
      lexical_cast<string>(neigh_vertex),
      affected_vertex);

  return;

}



void BGPProcess::for1(
    vertex_t affected_vertex,
    vertex_t neigh_vertex,
    tbb::concurrent_unordered_set<vertex_t>& new_changed_set,
    int cmp) {

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

  LOG4CXX_INFO(comp_peer_->logger_, "Compare -> "
      << current_preference << ", " << offered_preference );


#if 1
  const bool condition = offered_preference <= current_preference;

  if (cmp != condition) {
    printf("==================================================\n");
    printf("(Is, Should): (%d, %d) -- Vertex (%ld, %ld)\n",
        cmp, condition, affected_vertex, neigh_vertex);
    printf("==================================================\n");
  }
#endif

  if ( offered_preference <= current_preference ) {
    affected.sig_bgp_next[result]->operator()();
    return;
  }

#if 0
  if ( neigh.in_as_path(graph_, affected_vertex) ) {
    LOG4CXX_INFO(comp_peer_->logger_, "In AS PATH!")
    affected.sig_bgp_next[result]->operator()();
    return;
  }
#endif

  affected.set_next_hop(graph_, neigh_vertex);
  new_changed_set.insert(affected_vertex);

  affected.sig_bgp_next[result]->operator ()();
}



void BGPProcess::load_graph(string path, graph_t& graph) {

  dynamic_properties dp;
  std::ifstream file(path);

  dp.property("node_id", get(&Vertex::id_, graph));
  dp.property("key", get(&Edge::key_, graph));

  read_graphviz(file ,graph, dp,"node_id");
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

  printf("digraph G {\n");

  for (; current != last; ++current) {
    const auto& current_vertex = *current;
    Vertex& vertex = graph_[current_vertex];
    printf("%ld -> %ld;\n", vertex.id_, vertex.next_hop_);
  }

  printf("}\n");
}
