
#include <secret_sharing/secret.hpp>

#include <peer/input_peer.hpp>
#include <peer/comp_peer.hpp>
#include <peer/comp_peer_factory.hpp>

#include <log4cxx/patternlayout.h>
#include <log4cxx/propertyconfigurator.h>

#include <common.hpp>

#include <chrono>

LoggerPtr mainLogger(Logger::getLogger("all"));

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;

using boost::function;


void run_test2() {

  typedef std::chrono::high_resolution_clock clock_t;

  array<shared_ptr<comp_peer_t>, COMP_PEER_NUM> comp_peer_seq;
  shared_ptr<InputPeer> input_peer(new InputPeer());



  boost::thread_group worker_threads;

  io_service io;
  io_service::work work(io);

  CompPeer_factory factory;
  comp_peer_seq = factory.generate<COMP_PEER_NUM>(input_peer, io);

  input_peer->disseminate_bgp(comp_peer_seq);

  for (int i = 0; i < 4; i++) {
    worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  }


  boost::barrier* b = new boost::barrier(4);
  typedef function<bool()> Functor;
  Functor f = boost::bind(&boost::barrier::wait, b);

  for (auto& cp : comp_peer_seq) {
    cp->counter_ = 0;
  }


  const auto t1 = clock_t::now();

  for (auto& cp : comp_peer_seq) {
    BGPProcess* bgp = cp->bgp_.get();
    io.post(boost::phoenix::bind(&BGPProcess::start_callback, bgp, f));
  }

  b->wait();

  const auto t2 = clock_t::now();
  const auto duration = duration_cast<microseconds>(t2 - t1).count();
  std::cout << "The execution took " << duration << " microseconds." << std::endl;


  comp_peer_seq[0]->bgp_->print_result();

  io.stop();
  worker_threads.join_all();

}


void run_test1() {

  typedef std::chrono::high_resolution_clock clock_t;

  array<shared_ptr<comp_peer_t>, COMP_PEER_NUM> comp_peer_seq;
  shared_ptr<InputPeer> input_peer(new InputPeer());

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
  worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );

  CompPeer_factory factory;
  comp_peer_seq = factory.generate<COMP_PEER_NUM>(input_peer, io);

  //for(std::pair<string, int> pair: input_peer->plaintext_map_) {
    //InputPeer::lsb(pair.first, pair.second, comp_peer_seq);
  //}

  InputPeer::distribute_lsb(input_peer->plaintext_map_, comp_peer_seq);
  InputPeer::distribute_secrets(input_peer->plaintext_map_, comp_peer_seq);

  vector<string> circut = {"*", "C","*", "C", "+", "2", "A"};

  for (auto& cp : comp_peer_seq) {
    cp->counter_ = 0;
  }

  const auto t1 = clock_t::now();

  //for (auto& cp : comp_peer_seq) {
    //io.post(bind(&comp_peer_t::evaluate, cp.get(), "C", "B" ));
    //io.post(bind(&comp_peer_t::generate_random_bit, cp.get(), "R" ));
    //io.post(bind(&comp_peer_t::generate_random_bitwise_num, cp.get(), "BR" ));
    //io.post(bind(&comp_peer_t::evaluate, cp.get(), circut));

  //}

  result_thread.join();

  const auto t2 = clock_t::now();
  const auto duration = duration_cast<microseconds>(t2 - t1).count();

  LOG4CXX_INFO(mainLogger, "The execution took " << duration << " microseconds.")

  io.stop();
  worker_threads.join_all();

}


int main() {

  log4cxx::PropertyConfigurator::configure("apache.conf");

  run_test2();
}
