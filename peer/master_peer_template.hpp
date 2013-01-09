
#ifndef MASTER_PEER_TEMPLATE_HPP_
#define MASTER_PEER_TEMPLATE_HPP_

#include <peer/master_peer.hpp>

#include <numeric>
#include <iostream>
#include <sstream>

#include <boost/thread/locks.hpp>
#include <boost/assign.hpp>



MasterPeer::MasterPeer(
    size_t num,
    io_service& io) :
      Peer(io),
      graph_size_(num * COMP_PEER_NUM),
      peers_(0),
      peers_synchronized_(0),
      vertex_count_(0),
      started_(false)
    {

  master_server_ = shared_ptr<RPCServer>(new RPCServer(io, MASTER_PORT, this));
}



MasterPeer::~MasterPeer() {}



void MasterPeer::publish(Session* session, vector<vertex_t>& nodes) {

  boost::mutex::scoped_lock lock(mutex_);

  node_set_.insert( nodes.begin(), nodes.end() );

  if (started_) {

    peers_synchronized_ += 1;
    printf("Number of synchronized peers: %u\n", peers_synchronized_);

    if (peers_synchronized_ == all_sessions_.size()) {
      peers_synchronized_ = 0;

      for(auto s: master_server_->sessions_) {
        printf("Raising the barrier.\n");
        nodes_ = vector<vertex_t>(node_set_.begin(), node_set_.end());
        s->notify(nodes_);
      }

      node_set_.clear();
    }

  } else {

    vertex_count_ += nodes.size();
    all_sessions_.push_back(session);

    printf("Vertex count: %u\n", vertex_count_);

    if (vertex_count_ == graph_size_) {

      printf("Total number of peers participating: %u\n", all_sessions_.size());
      started_ = true;

      for(auto s: master_server_->sessions_) {
        nodes_ = vector<vertex_t>(node_set_.begin(), node_set_.end());
        s->notify(nodes_);
      }

      node_set_.clear();
    }

  }


}


#endif
