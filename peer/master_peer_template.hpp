
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



void MasterPeer::publish(vector<vertex_t>& nodes) {

  boost::mutex::scoped_lock lock(mutex_);

  nodes_.insert(nodes_.end(), nodes.begin(), nodes.end() );

  if (started_) {

    peer_count_round_ += 1;
    if (peer_count_round_ == (peer_count_/3)) {
      peer_count_round_ = 0;

      for(auto s: master_server_->sessions_)
        s->notify(nodes_);

      nodes_.clear();
    }

  } else {

    count_ += nodes.size();
    peer_count_ += 1;

    if (count_ == num_) {
      printf("Total number of peers participating: %u\n", peer_count_);
      started_ = true;

      for(auto s: master_server_->sessions_)
        s->notify(nodes_);

      nodes_.clear();
    }

  }


}


#endif
