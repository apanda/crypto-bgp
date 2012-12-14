#include <bgp/bgp.hpp>
#include <peer/comp_peer.hpp>




BGPProcess::BGPProcess(
    string path,
    shared_ptr<boost::barrier> bp,
    CompPeer<3>* comp_peer,
    io_service& io):
    comp_peer_(comp_peer),
    bp_(bp),
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

  for(auto it = affected_set.begin(); ;) {

    int suppose = 0;
    int count = 0;

    if((*it) == dst_vertex) {
      ++it;
      continue;
    }

    if(it == affected_set.end()) {
      break;
    }

    io_service_.post(
      boost::bind(
          &BGPProcess::process_neighbors_mpc,
          this, (*it),
          changed_set,
          boost::ref(new_changed_set),
          boost::ref(count)
      )
    );

    suppose++;
    ++it;


    /*

    if((*it) == dst_vertex) {

      boost::unique_lock<boost::mutex> lock(m_);
      while(count != suppose) {
        cv_.wait(lock);
      }

      ++it;
      continue;
    }

    if(it == affected_set.end()) {

      boost::unique_lock<boost::mutex> lock(m_);
      while(count != suppose) {
        cv_.wait(lock);
      }
      break;
    }


    io_service_.post(
      boost::bind(
          &BGPProcess::process_neighbors_mpc,
          this, (*it),
          changed_set,
          boost::ref(new_changed_set),
          boost::ref(count)
      )
    );

    suppose++;
    ++it;




    if((*it) == dst_vertex) {

      boost::unique_lock<boost::mutex> lock(m);
      while(count != suppose) {
        cv.wait(lock);
      }

      ++it;
      continue;
    }

    if(it == affected_set.end()) {

      boost::unique_lock<boost::mutex> lock(m);
      while(count != suppose) {
        cv.wait(lock);
      }
      break;
    }


    io_service_.post(
      boost::bind(
          &BGPProcess::process_neighbors_mpc,
          this, (*it),
          boost::ref(graph),
          boost::ref(changed_set),
          boost::ref(new_changed_set),
          boost::ref(count),
          boost::ref(m),
          boost::ref(cv)
      )
        );

    suppose++;
    ++it;




    if((*it) == dst_vertex) {

      boost::unique_lock<boost::mutex> lock(m);
      while(count != suppose) {
        cv.wait(lock);
      }

      ++it;
      continue;
    }

    if(it == affected_set.end()) {

      boost::unique_lock<boost::mutex> lock(m);
      while(count != suppose) {
        cv.wait(lock);
      }
      break;
    }


    io_service_.post(
      boost::bind(
          &BGPProcess::process_neighbors_mpc,
          this, (*it),
          boost::ref(graph),
          boost::ref(changed_set),
          boost::ref(new_changed_set),
          boost::ref(count),
          boost::ref(m),
          boost::ref(cv)
      )
        );

    suppose++;
    ++it;

    if((*it) == dst_vertex) {

      boost::unique_lock<boost::mutex> lock(m);
      while(count != suppose) {
        cv.wait(lock);
      }

      ++it;
      continue;
    }

    if(it == affected_set.end()) {

      boost::unique_lock<boost::mutex> lock(m);
      while(count != suppose) {
        cv.wait(lock);
      }
      break;
    }


    io_service_.post(
      boost::bind(
          &BGPProcess::process_neighbors_mpc,
          this, (*it),
          boost::ref(graph),
          boost::ref(changed_set),
          boost::ref(new_changed_set),
          boost::ref(count),
          boost::ref(m),
          boost::ref(cv)
      )
        );

    suppose++;
    ++it;
*/

    boost::unique_lock<boost::mutex> lock(m_);
    while(count != suppose) {
      cv_.wait(lock);
      //printf("Woken up!\n");
    }

/*
    comp_peer_->mutex_map_2.clear();
    comp_peer_->cv_map_2.clear();
    comp_peer_->couter_map_2.clear();
*/
  }



  for(const vertex_t vertex: new_changed_set) {
    auto neighbors = adjacent_vertices(vertex, graph_);
    new_affected_set.insert(neighbors.first, neighbors.second);
  }

  if(new_changed_set.empty()) return;

  next_iteration(dst_vertex, graph, new_affected_set, new_changed_set);


}


void BGPProcess::for1(
    vertex_t affected_vertex,
    vertex_t neigh_vertex,
    tbb::concurrent_unordered_set<vertex_t>& new_changed_set,
    int cmp) {

  Vertex& affected = graph_[affected_vertex];
  const auto current_preference = affected.current_next_hop_preference(graph_);

  auto offer_it = affected.preference_.find(neigh_vertex);
  BOOST_ASSERT(offer_it != affected.preference_.end());
  const auto offered_preference = offer_it->second;

  LOG4CXX_INFO(comp_peer_->logger_, "Compare -> "
      << current_preference << ", " << offered_preference );

  const bool condition = offered_preference <= current_preference;

  if (cmp != condition) {
    printf("==================================================\n");
    printf("(Is, Should): (%d, %d) -- Vertex (%ld, %ld)\n",
        cmp, condition, affected_vertex, neigh_vertex);
    printf("==================================================\n");
  }


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


  if ( offered_preference <= current_preference ) {
    comp_peer_->sig_map_3[affected_vertex][result]->operator ()();
    return;
  }

  affected.set_next_hop(graph_, neigh_vertex);
  //printf("%ld next hop set to: %ld\n", affected_vertex, affected.next_hop_);
  new_changed_set.insert(affected_vertex);

  comp_peer_->sig_map_3[affected_vertex][result]->operator ()();
}




void BGPProcess::for0(
    const vertex_t affected_vertex,
    tbb::concurrent_unordered_set<vertex_t> changed_set,
    tbb::concurrent_unordered_set<vertex_t>& new_changed_set,
    int& count,
    int& cnt,
    std::pair<graph_t::adjacency_iterator, graph_t::adjacency_iterator>& neighbors) {


  if (cnt != 0) {
    ++neighbors.first;
  }
  cnt++;

  auto next = boost::bind(&BGPProcess::for0, this,
        affected_vertex,
        changed_set,
        boost::ref(new_changed_set),
        boost::ref(count),
        boost::ref(cnt),
        boost::ref(neighbors)
      );

  if (neighbors.first == neighbors.second) {
    boost::unique_lock<boost::mutex> lock(m_);
    count++;

    cv_.notify_all();
    return;
  }

  Vertex& affected = graph_[affected_vertex];
  const vertex_t neigh_vertex = *(neighbors.first);

  if(changed_set.find(neigh_vertex) == changed_set.end()) {
    next();
    return;
  }

  //printf("Vertex: (%ld -> %ld)\n", affected_vertex, neigh_vertex);

  //boost::recursive_mutex& m = *(comp_peer_->mutex_map_[neigh_vertex]);
  //Peer::mutex_t::scoped_lock lock(m);

  Vertex& neigh = graph_[neigh_vertex];

  if ( neigh.in_as_path(graph_, affected_vertex) ) {
    next();
    return;
  }

  //comp_peer_->sig_map_3

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

  comp_peer_->sig_map_3[affected_vertex][result] =
      shared_ptr<boost::signals2::signal<void()> >(new boost::signals2::signal<void()>);
  comp_peer_->sig_map_3[affected_vertex][result]->connect(1, next);


  comp_peer_->sig_map_2[affected_vertex][result] =
      shared_ptr<boost::signals2::signal<void(int)> >(new boost::signals2::signal<void(int)>);
  comp_peer_->sig_map_2[affected_vertex][result]->connect(
      boost::bind(&BGPProcess::for1, this,
              affected_vertex,
              neigh_vertex,
              boost::ref(new_changed_set),
              _1
            )
  );

  const int cmp = comp_peer_->compare0(
      lexical_cast<string>(affected.next_hop_),
      lexical_cast<string>(neigh_vertex),
      affected_vertex);

  return;

}





void BGPProcess::process_neighbors_mpc(
    const vertex_t affected_vertex,
    tbb::concurrent_unordered_set<vertex_t> changed_set,
    tbb::concurrent_unordered_set<vertex_t>& new_changed_set,
    int& count) {

  Vertex& affected = graph_[affected_vertex];
  auto neighbors = adjacent_vertices(affected_vertex, graph_);

  int cnt = 0;

  for0(affected_vertex, changed_set, new_changed_set, count, cnt, neighbors);


/*
  boost::unique_lock<boost::mutex> lock(m_);
  count++;

  cv_.notify_all();
*/

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
