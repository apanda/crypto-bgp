
#include <secret_sharing/secret.hpp>

#include <peer/input_peer.hpp>
#include <peer/comp_peer.hpp>
#include <peer/comp_peer_factory.hpp>

#include <common.hpp>

#include <chrono>

int main() {

  log4cxx::BasicConfigurator::configure();

  typedef std::chrono::high_resolution_clock clock_t;

  array<shared_ptr<comp_peer_t>, COMP_PEER_NUM> comp_peer_seq;
  shared_ptr<Input_peer> input_peer(new Input_peer());

  input_peer->plaintext_map_ = {
      {"A", 4},
      {"B", 4},
      {"C", 2}
  };

  boost::thread result_thread(&Input_peer::result<COMP_PEER_NUM>, input_peer);
  boost::thread_group worker_threads;

  io_service io;
  io_service::work work(io);

  worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );

  Comp_peer_factory factory;
  comp_peer_seq = factory.generate<COMP_PEER_NUM>(input_peer);

  Input_peer::distribute_secrets(input_peer->plaintext_map_, comp_peer_seq);
  vector<string> circut = {"*", "C", "*", "C", "*", "B", "A"};

  auto t1 = clock_t::now();

  for (auto& cp : comp_peer_seq) {
    io.post(bind(&comp_peer_t::execute, cp.get(), circut));
  }

  result_thread.join();

  auto t2 = clock_t::now();

  std::cout << "The execution took " <<
  std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() <<
  " microseconds." << std::endl;

  io.stop();

  worker_threads.interrupt_all();
  result_thread.interrupt();
}
