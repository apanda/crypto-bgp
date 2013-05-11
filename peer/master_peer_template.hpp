
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
      started_(false),
      graph_size_(num * COMP_PEER_NUM),
      peers_(0),
      peers_synchronized_(0),
      vertex_count_(0)
    {

  LOG4CXX_INFO(logger_, "Graph size: " << num << " nodes.");
  master_server_ = shared_ptr<RPCServer>(new RPCServer(io, MASTER_PORT, this));
}



MasterPeer::~MasterPeer() {}


void MasterPeer::publish(Session* session, sync_init si) {

  boost::mutex::scoped_lock lock(mutex_);

  node_set_.insert( si.nodes_.begin(), si.nodes_.end() );
  LOG4CXX_INFO(logger_, "Received " << si.nodes_.size() << " nodes from " << si.hostname_);

  auto& m = sync_response_.hostname_mappings_[si.id_];

  for(auto v: si.nodes_) {
     m[v] = si.hostname_;
  }

  vertex_count_ += si.nodes_.size();
  all_sessions_.push_back(session);

  LOG4CXX_INFO(logger_, "Vertex count: " << vertex_count_);

  if (vertex_count_ == graph_size_) {

    LOG4CXX_INFO(logger_, "Total number of peers participating: " << all_sessions_.size());
    started_ = true;

    for(auto s: master_server_->sessions_) {
      s->sync_response(sync_response_);
    }

    node_set_.clear();
  }

}



void MasterPeer::publish(Session* session, vector<vertex_t>& nodes, size_t id) {

  boost::mutex::scoped_lock lock(mutex_);

  node_set_.insert( nodes.begin(), nodes.end() );

  if (started_) {

    peers_synchronized_ += 1;
    const string address = session->socket_.remote_endpoint().address().to_string();
    LOG4CXX_INFO(logger_, "Number of synchronized peers: " << peers_synchronized_
        << " (" << address << ")");

    if (peers_synchronized_ == all_sessions_.size()) {
      peers_synchronized_ = 0;

      LOG4CXX_INFO(logger_, "Raising the barrier.");
      nodes_ = vector<vertex_t>(node_set_.begin(), node_set_.end());
      pair<char*, size_t> data = Session::contruct_notification(nodes_);

      for(Session* s: master_server_->sessions_) {
        s->write_impl(data.first, data.second, s->socket_);
      }

      node_set_.clear();
    }

  } else {

    vertex_count_ += nodes.size();
    all_sessions_.push_back(session);

    if (vertex_count_ == graph_size_) {

      LOG4CXX_INFO(logger_, "Total number of peers participating: " << all_sessions_.size());
      started_ = true;

      nodes_ = vector<vertex_t>(node_set_.begin(), node_set_.end());
      pair<char*, size_t> data = Session::contruct_notification(nodes_);

      for(Session* s: master_server_->sessions_) {
        s->write_impl(data.first, data.second, s->socket_);
      }

      node_set_.clear();
    }

  }
}


#endif
