
#include <secret_sharing/secret.hpp>

#include <peer/input_peer.hpp>
#include <peer/comp_peer.hpp>
#include <peer/comp_peer_factory.hpp>

#include <log4cxx/patternlayout.h>

#include <common.hpp>

#include <chrono>

LoggerPtr mainLogger(Logger::getLogger("all"));

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;






void run_test2() {

  typedef std::chrono::high_resolution_clock clock_t;

  array<shared_ptr<comp_peer_t>, COMP_PEER_NUM> comp_peer_seq;
  shared_ptr<InputPeer> input_peer(new InputPeer());

  CompPeer_factory factory;
  comp_peer_seq = factory.generate<COMP_PEER_NUM>(input_peer);

  input_peer->disseminate_bgp(comp_peer_seq);

}


void run_test1() {

  typedef std::chrono::high_resolution_clock clock_t;

  array<shared_ptr<comp_peer_t>, COMP_PEER_NUM> comp_peer_seq;
  shared_ptr<InputPeer> input_peer(new InputPeer());

  input_peer->disseminate_bgp(comp_peer_seq);

  input_peer->plaintext_map_ = {
      {"A", 2},
      {"B", 3},
      {"C", 4}
  };

  boost::thread result_thread(&InputPeer::result<COMP_PEER_NUM>, input_peer);
  boost::thread_group worker_threads;

  io_service io;
  io_service::work work(io);

  worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );

  CompPeer_factory factory;
  comp_peer_seq = factory.generate<COMP_PEER_NUM>(input_peer);

  for(std::pair<string, int> pair: input_peer->plaintext_map_) {
    //InputPeer::lsb(pair.first, pair.second, comp_peer_seq);
  }

  InputPeer::distribute_lsb(input_peer->plaintext_map_, comp_peer_seq);
  InputPeer::distribute_secrets(input_peer->plaintext_map_, comp_peer_seq);

  vector<string> circut = {"*", "C","*", "C", "+", "2", "A"};

  for (auto& cp : comp_peer_seq) {
    cp->counter_ = 0;
  }

  const auto t1 = clock_t::now();

  for (auto& cp : comp_peer_seq) {
    io.post(bind(&comp_peer_t::evaluate, cp.get(), "C", "B" ));
    //io.post(bind(&comp_peer_t::generate_random_bit, cp.get(), "R" ));
    //io.post(bind(&comp_peer_t::generate_random_bitwise_num, cp.get(), "BR" ));
    //io.post(bind(&comp_peer_t::evaluate, cp.get(), circut));

  }

  result_thread.join();

  const auto t2 = clock_t::now();
  const auto duration = duration_cast<microseconds>(t2 - t1).count();

  LOG4CXX_INFO(mainLogger, "The execution took " << duration << " microseconds.")

  io.stop();
  worker_threads.join_all();

}


int main() {

  mainLogger->setLevel(log4cxx::Level::getInfo());

  log4cxx::BasicConfigurator::configure();
  log4cxx::PatternLayoutPtr patternLayout = new log4cxx::PatternLayout();
  patternLayout->setConversionPattern("%m%n");

  run_test2();
}
