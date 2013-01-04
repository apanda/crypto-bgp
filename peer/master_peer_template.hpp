
#ifndef MASTER_PEER_TEMPLATE_HPP_
#define MASTER_PEER_TEMPLATE_HPP_

#include <peer/master_peer.hpp>

#include <numeric>
#include <iostream>
#include <sstream>

#include <boost/thread/locks.hpp>
#include <boost/assign.hpp>



MasterPeer::MasterPeer(
    size_t id,
    io_service& io) :
      Peer(id + 10000, io)
    {

  master_server_ = shared_ptr<RPCServer>(new RPCServer(io, MASTER_PORT, this));
}



MasterPeer::~MasterPeer() {}



void MasterPeer::publish(std::string key, int64_t value, vertex_t v) {

  printf("Started %ld vertices!\n", v);

  for(auto s: master_server_->sessions_) {
    s->notify("Go!", 0, 1);
  }

}


#endif
