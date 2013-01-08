
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
      num_(num * COMP_PEER_NUM),
      peers_(0),
      peer_count_(0),
      peer_count_round_(0),
      count_(0),
      started_(false)
    {

  master_server_ = shared_ptr<RPCServer>(new RPCServer(io, MASTER_PORT, this));
}



MasterPeer::~MasterPeer() {}



void MasterPeer::publish(Session* session, vector<vertex_t>& nodes) {

  boost::mutex::scoped_lock lock(mutex_);

  node_set_.insert( nodes.begin(), nodes.end() );

  if (started_) {

    peer_count_round_ += 1;
    printf("peer_count_round_ %u\n", peer_count_round_);
    if (peer_count_round_ == sessions_.size()) {
      peer_count_round_ = 0;

      for(auto s: master_server_->sessions_) {
        printf("syncing up with a comp peer\n");
        nodes_ = vector<vertex_t>(node_set_.begin(), node_set_.end());
        s->notify(nodes_);
      }

      node_set_.clear();
    }

  } else {

    count_ += nodes.size();
    sessions_.push_back(session);
    peer_count_++;

    printf("count %u\n", count_);

    if (count_ == num_) {
      printf("Total number of peers participating: %u\n", sessions_.size());
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
