#include <bgp/bgp.hpp>
#include <peer/comp_peer.hpp>




BGPProcess::BGPProcess(string path, shared_ptr<boost::barrier> bp, CompPeer<3>* comp_peer):
    comp_peer_(comp_peer),
    bp_(bp) {
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
  set<vertex_t> changed;

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
    set<vertex_t>& changed_set) {

  boost::thread_group tg;

  //std::cout << "Next iteration... " << changed_set.size() << " " << affected_set.size() << std::endl;

  set<vertex_t> new_affected_set;
  set<vertex_t> new_changed_set;


  for(auto it = affected_set.begin(); ;) {
    //printf("%ld\n", (*it));

    if((*it) == dst_vertex) {
      ++it;
      continue;
    }

    if(it == affected_set.end()) {
      tg.join_all();
      break;;
    }


    tg.add_thread( new boost::thread(
      boost::bind(
          &BGPProcess::process_neighbors_mpc,
          this, (*it),
          boost::ref(graph),
          boost::ref(changed_set),
          boost::ref(new_changed_set)
      )
    ));

    ++it;



    if((*it) == dst_vertex) {
      ++it;
      continue;
    }

    if(it == affected_set.end()) {
      tg.join_all();
      break;
    }


    tg.add_thread( new boost::thread(
      boost::bind(
          &BGPProcess::process_neighbors_mpc,
          this, (*it),
          boost::ref(graph),
          boost::ref(changed_set),
          boost::ref(new_changed_set)
      )
    ));

    ++it;

    tg.join_all();
  }
  /*
  for(const vertex_t affected_vertex: affected_set) {
    if(affected_vertex == dst_vertex) continue;

    tg.add_thread( new boost::thread(
      boost::bind(
          &BGPProcess::process_neighbors_mpc,
          this, affected_vertex,
          boost::ref(graph),
          boost::ref(changed_set),
          boost::ref(new_changed_set)
      )
    ));

    tg.join_all();
    //process_neighbors_mpc(affected_vertex, graph, changed_set, new_changed_set);
  }
*/


  for(const vertex_t vertex: new_changed_set) {
    auto neighbors = adjacent_vertices(vertex, graph);
    new_affected_set.insert(neighbors.first, neighbors.second);
  }

  if(new_changed_set.empty()) return;

  next_iteration(dst_vertex, graph, new_affected_set, new_changed_set);


}



void BGPProcess::process_neighbors_mpc(
    const vertex_t affected_vertex,
    graph_t& graph,
    set<vertex_t>& changed_set,
    set<vertex_t>& new_changed_set) {

  Vertex& affected = graph[affected_vertex];
  auto neighbors = adjacent_vertices(affected_vertex, graph);

  for(; neighbors.first != neighbors.second; ++neighbors.first) {
    const vertex_t neigh_vertex = *(neighbors.first);

    if(changed_set.find(neigh_vertex) != changed_set.end()) {

      //printf("Vertex: (%ld -> %ld)\n", affected_vertex, neigh_vertex);

      Vertex& neigh = graph[neigh_vertex];

      if ( neigh.in_as_path(graph, affected_vertex) ) continue;

      const auto current_preference = affected.current_next_hop_preference(graph);

      auto offer_it = affected.preference_.find(neigh_vertex);
      BOOST_ASSERT(offer_it != affected.preference_.end());
      const auto offered_preference = offer_it->second;

      const bool condition = offered_preference <= current_preference;
      const int cmp = comp_peer_->compare(
          lexical_cast<string>(affected.next_hop_),
          lexical_cast<string>(neigh_vertex),
          affected_vertex);

      if (cmp != condition) {
        printf("==================================================\n");
        printf("(Is, Should): (%d, %d) -- Vertex (%ld, %ld)\n", cmp, condition, affected_vertex, neigh_vertex);
        printf("==================================================\n");
      }

      if ( offered_preference <= current_preference ) continue;

      affected.set_next_hop(graph, neigh_vertex);
      //printf("%ld next hop set to: %ld\n", affected_vertex, affected.next_hop_);
      new_changed_set.insert(affected_vertex);
    }
  }
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
