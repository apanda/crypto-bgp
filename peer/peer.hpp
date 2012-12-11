#ifndef PEER_HPP_
#define PEER_HPP_

#include <common.hpp>
#include <net/rpc_client.hpp>
#include <net/rpc_server.hpp>

#include <atomic>
#include <unordered_map>

#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/locks.hpp>
#include <boost/shared_ptr.hpp>

#include <tbb/compat/condition_variable>
#include <tbb/concurrent_unordered_map.h>

typedef int64_t plaintext_t;
typedef string symbol_t;

class RPCServer;

class Peer {
public:
  Peer(const short port, io_service& io);
  ~Peer();

  template<class PeerSeq>
  void distribute_secret(
      const symbol_t key, const int64_t value, const vertex_t vertex,
      PeerSeq comp_peers);

  template<class Values, class PeerSeq>
  static void distribute_shares(
      const symbol_t key, const Values values, const int64_t secret, const vertex_t vertex,
      PeerSeq comp_peers);

  void publish(std::string key,  int64_t value, vertex_t vertex);
  void print_values();

  typedef tbb::concurrent_unordered_map<int, int64_t> inter_map_t;
  typedef tbb::concurrent_unordered_map<symbol_t, int64_t> value_map_t;

  //typedef tbb::mutex mutex_t;
  //typedef tbb::interface5::unique_lock<tbb::mutex> lock_t;
  //typedef tbb::interface5::condition_variable condition_variable_t;

  typedef boost::mutex mutex_t;
  typedef boost::unique_lock<mutex_t> lock_t;
  typedef boost::condition_variable condition_variable_t;


  io_service& io_service_;
  //io_service::work work_;
  RPCServer* server_;

  int counter_;

  std::unordered_map<int, value_map_t > vertex_value_map_;

  std::unordered_map<int, int > couter_map_;
  std::unordered_map<int, shared_ptr<mutex_t> > mutex_map_;
  std::unordered_map<int, shared_ptr< condition_variable_t> > cv_map_;

  std::unordered_map<
    int, std::unordered_map<string, int>
  > couter_map_2;

  std::unordered_map<
    int, tbb::concurrent_unordered_map<string, shared_ptr<mutex_t> >
  > mutex_map_2;

  std::unordered_map<
    int, tbb::concurrent_unordered_map<string, shared_ptr<condition_variable_t> >
  > cv_map_2;


  std::unordered_map<int, boost::signals2::signal<void (string)> > sig_map_;

  mutex_t barrier_mutex_;
  mutex_t __mutex;

  std::string recombination_key_;

  inter_map_t intermediary_;

  boost::thread_group tg_;

  static log4cxx::LoggerPtr logger_;
};



#include <peer/peer_template.hpp>

#endif /* PEER_HPP_ */
