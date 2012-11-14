#ifndef PEER_HPP_
#define PEER_HPP_

#include <common.hpp>
#include <net/rpc_client.hpp>
#include <net/rpc_server.hpp>

#include <atomic>
#include <unordered_map>

#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/condition.hpp>

#include <tbb/concurrent_unordered_map.h>
#include <tbb/mutex.h>

typedef  int64_t plaintext_t;
typedef string symbol_t;

class RPCServer;

class Peer {
public:
  Peer(const short port);
  ~Peer();

  template<class PeerSeq>
  void distribute_secret(
      const symbol_t key, const int64_t value,
      PeerSeq comp_peers);

  template<class Values, class PeerSeq>
  static void distribute_shares(
      const symbol_t key, const Values values, const int64_t secret,
      PeerSeq comp_peers);

  void publish(std::string key,  int64_t value);
  void print_values();

  typedef tbb::concurrent_unordered_map<int, int64_t> inter_map_t;
  typedef std::unordered_map<symbol_t, int64_t> value_map_t;

  io_service io_service_;
  RPCServer* server_;

  std::atomic<int> counter_;
  boost::shared_mutex barrier_mutex_;

  tbb::mutex __mutex;

  std::string recombination_key_;

  inter_map_t intermediary_;
  value_map_t values_;

  boost::thread_group tg_;

  static log4cxx::LoggerPtr logger_;
};

#include "peer_template.hpp"

#endif /* PEER_HPP_ */
