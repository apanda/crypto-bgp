#include <bgp/bgp.hpp>
#include <peer/comp_peer.hpp>




BGPProcess::BGPProcess(string path, CompPeer<3>* comp_peer):
    comp_peer_(comp_peer) {
  load_graph(path, graph_);
  init(graph_);
}



void BGPProcess::start(graph_t& graph) {

  Vertex& dst = graph[0];

  dst.next_hop_ = 0;
  dst.as_path_.push(dst.id_);
  dst.as_path_set_.insert(dst.id_);

  set<vertex_t> affected;
  set<vertex_t> changed;

  changed.insert(0);
  for(const vertex_t& vertex: dst.neigh_) {
    affected.insert(vertex);
  }

  next_iteration(0, graph, affected, changed);

}



void BGPProcess::init(graph_t& graph) {

  auto iter = vertices(graph);
  auto last = iter.second;
  auto current = iter.first;

  for (; current != last; ++current) {
    const auto& current_vertex = *current;
    Vertex& vertex = graph[current_vertex];

    vertex.set_neighbors(graph);
    vertex.set_preference();
  }

}



void BGPProcess::next_iteration(
    const vertex_t dst_vertex,
    graph_t& graph,
    set<vertex_t>& affected_set,
    set<vertex_t>& changed_set) {

  std::cout << "Next itteration..." << std::endl;

  set<vertex_t> new_affected_set;
  set<vertex_t> new_changed_set;

  for(const vertex_t affected_vertex: affected_set) {
    std::cout << "Current vertex: " << affected_vertex << std::endl;
    if(affected_vertex == dst_vertex) continue;
    process_neighbors(affected_vertex, graph, changed_set, new_changed_set);
  }

  for(const vertex_t vertex: new_changed_set) {
    auto neighbors = adjacent_vertices(vertex, graph);
    new_affected_set.insert(neighbors.first, neighbors.second);
  }

  if( !new_changed_set.empty() ) {
    print_state(graph, new_affected_set, new_changed_set);
    next_iteration(dst_vertex, graph, new_affected_set, new_changed_set);
  }

}



void BGPProcess::process_neighbors(
    const vertex_t affected_vertex,
    graph_t& graph,
    set<vertex_t>& changed_set,
    set<vertex_t>& new_changed_set) {

  Vertex& affected = graph[affected_vertex];
  auto neighbors = adjacent_vertices(affected_vertex, graph);

  for(; neighbors.first != neighbors.second; ++neighbors.first) {
    const vertex_t& neigh_vertex = *(neighbors.first);
    std::cout << "\tNeighbor: " << neigh_vertex << std::endl;

    if(changed_set.find(neigh_vertex) != changed_set.end()) {
      Vertex& neigh = graph[neigh_vertex];

      const auto current_preference = affected.current_next_hop_preference(graph);
      const auto offered_preference = affected.preference_[neigh_vertex];
      std::cout << "\tCurrent preference: " << current_preference << std::endl;
      std::cout << "\tOffered preference: " << offered_preference << std::endl;

      if ( offered_preference <= current_preference ) continue;
      if ( neigh.in_as_path(graph, affected_vertex) ) continue;

      affected.set_next_hop(graph, neigh_vertex);
      new_changed_set.insert(affected_vertex);
    }
  }
}




void BGPProcess::process_neighbors_mpc(
    const vertex_t affected_vertex,
    graph_t& graph,
    set<vertex_t>& changed_set,
    set<vertex_t>& new_changed_set) {

  Vertex& affected = graph[affected_vertex];
  auto neighbors = adjacent_vertices(affected_vertex, graph);

  for(; neighbors.first != neighbors.second; ++neighbors.first) {
    const vertex_t& neigh_vertex = *(neighbors.first);
    std::cout << "\tNeighbor: " << neigh_vertex << std::endl;

    if(changed_set.find(neigh_vertex) != changed_set.end()) {
      Vertex& neigh = graph[neigh_vertex];

      comp_peer_->values_ = affected.values_;

      const auto current_preference = affected.next_hop_;
      const auto offered_preference = neigh_vertex;

      comp_peer_->compare(
          lexical_cast<string>(current_preference),
          lexical_cast<string>(offered_preference));

      if (current_preference < offered_preference) {
        continue;
      }

      if ( neigh.in_as_path(graph, affected_vertex) ) continue;

      affected.set_next_hop(graph, neigh_vertex);
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
