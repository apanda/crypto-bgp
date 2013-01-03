
#include <boost/program_options.hpp>
#include <secret_sharing/secret.hpp>

#include <peer/input_peer.hpp>
#include <peer/master_peer.hpp>
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

  io_service io(20);
  io_service::work work(io);

  CompPeer_factory factory;
  comp_peer_seq = factory.generate<COMP_PEER_NUM>(input_peer, io);


  BGPProcess bgp("scripts/dot.dot", NULL, io);
  graph_t& input_graph = bgp.graph_;

  shared_ptr<RPCClient> master( new RPCClient(io, "localhost", MASTER_PORT) );

  input_peer->disseminate_bgp(comp_peer_seq, input_graph);
  input_peer->start_listeners(comp_peer_seq, input_graph);

  master->publish("", 1, 1);

  input_peer->start_clients(comp_peer_seq, input_graph);

  for (int i = 0; i < THREAD_COUNT; i++) {
    worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  }


  boost::barrier* b = new boost::barrier(4);
  typedef function<bool()> Functor;
  Functor f = boost::bind(&boost::barrier::wait, b);

  const auto t1 = clock_t::now();

  for (auto& cp : comp_peer_seq) {
    BGPProcess* bgp = cp->bgp_.get();
    io.post(boost::phoenix::bind(&BGPProcess::start_callback, bgp, f));
  }

  b->wait();

  const auto t2 = clock_t::now();
  const auto duration = duration_cast<milliseconds>(t2 - t1).count();
  std::cout << "The execution took " << duration << " ms." << std::endl;


  comp_peer_seq[0]->bgp_->print_result();

  io.stop();
  worker_threads.join_all();

}



int main(int argc, char *argv[]) {

  namespace po = boost::program_options;

  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ("threads", po::value<int>(), "total number of threads")
      ("tasks", po::value<int>(), "total number of tasks per iteration")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
      std::cout << desc << std::endl;;
      return 1;
  }


  if (vm.count("threads")) {
    THREAD_COUNT = vm["threads"].as<int>();
  }

  if (vm.count("tasks")) {
    TASK_COUNT = vm["tasks"].as<int>();
  }

  log4cxx::PropertyConfigurator::configure("apache.conf");

  run_test2();
}
