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
#include <unordered_set>

class MasterPeer : public Peer {
public:

  MasterPeer(
      size_t id,
      io_service& io);
  ~MasterPeer();

  void publish(Session* session, sync_init si);
  void publish(Session* session, vector<update_vertex_t>& nodes, size_t id = 0);

  boost::mutex mutex_;
  bool started_;

  size_t graph_size_;
  size_t peers_;
  size_t peers_synchronized_;
  size_t vertex_count_;

  vector<update_vertex_t> nodes_;
  std::set<update_vertex_t> node_set_;

  std::vector<Session*> all_sessions_;
  sync_response sync_response_;

  shared_ptr<RPCServer> master_server_;
};

typedef CompPeer<COMP_PEER_NUM> comp_peer_t;


#include <peer/master_peer_template.hpp>

#endif /* COMPPEER_HPP_ */
