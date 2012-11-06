#ifndef PEER_HPP_
#define PEER_HPP_

#include "common.hpp"
#include "net/rpc_client.hpp"
#include "net/rpc_server.hpp"

#include <atomic>

#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/condition.hpp>

#include <tbb/concurrent_unordered_map.h>


typedef int plaintext_t;
typedef string symbol_t;

class RPCServer;

class Peer {
public:
  Peer(const short port);

  template<class PeerSeq>
  void distribute_secret(
      const symbol_t key, const int value,
      PeerSeq comp_peers);

  template<class Values, class PeerSeq>
  static void distribute_shares(
      const symbol_t key, const Values values,
      PeerSeq comp_peers);

  void publish(std::string key, int value);
  void print_values();

  typedef tbb::concurrent_unordered_map<int, int> inter_map_t;
  typedef tbb::concurrent_unordered_map <symbol_t, int> value_map_t;

  io_service io_service_;
  RPCServer* server_;

  std::atomic<int> counter_;

  boost::mutex mutex_;
  boost::unique_lock<boost::mutex> lock_;
  boost::condition_variable condition_;

  std::string recombination_key_;

  inter_map_t intermediary_;
  value_map_t values_;

  static log4cxx::LoggerPtr logger_;
};

#include "peer_template.hpp"

#endif /* PEER_HPP_ */
