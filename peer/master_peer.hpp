#ifndef MASTER_PEER_HPP_
#define MASTER_PEER_HPP_

#include <common.hpp>

#include <map>
#include <deque>
#include <atomic>

#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_queue.h>

#include <boost/array.hpp>
#include <boost/shared_ptr.hpp>

#include <gsl/gsl_blas.h>

#include <bgp/bgp.hpp>
#include <peer/peer.hpp>
#include <peer/input_peer.hpp>


class MasterPeer : public Peer {
public:

  MasterPeer(
      size_t id,
      io_service& io);
  ~MasterPeer();

  void publish(std::string key,  int64_t value, vertex_t vertex);

  shared_ptr<RPCServer> master_server_;
};

typedef CompPeer<COMP_PEER_NUM> comp_peer_t;


#include <peer/master_peer_template.hpp>

#endif /* COMPPEER_HPP_ */
