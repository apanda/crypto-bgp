
#include <boost/program_options.hpp>
#include <secret_sharing/secret.hpp>

#include <peer/input_peer.hpp>
#include <peer/master_peer.hpp>
#include <peer/comp_peer.hpp>
#include <peer/comp_peer_factory.hpp>

#include <bgp/vertex.hpp>

#include <log4cxx/patternlayout.h>
#include <log4cxx/propertyconfigurator.h>

#include <common.hpp>

#include <chrono>

LoggerPtr mainLogger(Logger::getLogger("all"));

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;

using boost::function;

io_service io(20);
io_service::work work(io);

void run_master() {

  typedef std::chrono::high_resolution_clock clock_t;

  array<shared_ptr<comp_peer_t>, COMP_PEER_NUM> comp_peer_seq;
  shared_ptr<InputPeer> input_peer(new InputPeer(io));

  graph_t graph;
  BGPProcess::load_graph("scripts/dot.dot", graph);

  MasterPeer mp(graph.m_vertices.size(), io);

  boost::thread_group worker_threads;

  worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  worker_threads.join_all();
}



void run_mpc() {

  typedef std::chrono::high_resolution_clock clock_t;

  array<shared_ptr<comp_peer_t>, COMP_PEER_NUM> comp_peer_seq;
  shared_ptr<InputPeer> input_peer(new InputPeer(io));

  boost::thread_group worker_threads;

  typedef function<bool()> functor_t;

  boost::barrier* b = new boost::barrier(COMP_PEER_IDS.size() + 1);
  functor_t f = boost::bind(&boost::barrier::wait, b);

  for (int i = 0; i < THREAD_COUNT; i++) {
    worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  }


  CompPeer_factory factory;
  comp_peer_seq = factory.generate<COMP_PEER_NUM>(input_peer, io);


  BGPProcess bgp("scripts/dot.dot", NULL, io);
  graph_t& input_graph = bgp.graph_;

  shared_ptr<RPCClient> master( new RPCClient(io, MASTER_ADDRESS, MASTER_PORT) );
  bgp.master_ = master;

  input_peer->disseminate_bgp(comp_peer_seq, input_graph);
  auto nodes = input_peer->start_listeners(comp_peer_seq, input_graph);


  master->barrier_ = new boost::barrier(2);

  for(auto& cp: comp_peer_seq) {
    if (COMP_PEER_IDS.find(cp->id_) == COMP_PEER_IDS.end()) continue;
    cp->bgp_->master_ = master;
    master->sync(nodes);
  }

  sleep(1);

  master->barrier_ ->wait();

  LOG4CXX_INFO(mainLogger, "Master has raised the barrier.");

  master->barrier_ = new boost::barrier(COMP_PEER_IDS.size() + 1);

  input_peer->start_clients(comp_peer_seq, input_graph);

  LOG4CXX_INFO(mainLogger, "All clients have been started.");

  for (auto& cp : comp_peer_seq) {
    if (COMP_PEER_IDS.find(cp->id_) == COMP_PEER_IDS.end()) continue;
    BGPProcess* bgp = cp->bgp_.get();
    io.post(boost::phoenix::bind(&BGPProcess::start_callback, bgp, f));
  }

  const auto t1 = clock_t::now();

  b->wait();

  const auto t2 = clock_t::now();
  const auto duration = duration_cast<milliseconds>(t2 - t1).count();

  comp_peer_seq[0]->bgp_->print_result();
  std::cout << "The execution took " << duration << " ms." << std::endl;

  io.stop();
  worker_threads.join_all();

}



int main(int argc, char *argv[]) {

  namespace po = boost::program_options;

  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ("master", "start the master peer")
      ("threads", po::value<int>(), "total number of threads")
      ("tasks", po::value<int>(), "total number of tasks per iteration")
      ("master-host", po::value<string>(), "master address")
      ("start", po::value<int>(), "staring vertex")
      ("end", po::value<int>(), "ending vertex")
      ("id", po::value<vector<int>>()->multitoken(), "computational peer id")
      ("host", po::value<vector<string>>()->multitoken(), "hosts associated with the computational peers")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  log4cxx::PropertyConfigurator::configure("apache.conf");

  if (vm.count("master")) {
    run_master();
    return 0;
  }

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

  if (vm.count("start")) {
    VERTEX_START = vm["start"].as<int>();
  }

  if (vm.count("end")) {
    VERTEX_END = vm["end"].as<int>();
  }

  if (vm.count("id")) {
    vector<int> ids =  vm["id"].as<vector<int>>();
    for(size_t i = 0; i < ids.size(); i++) {
      COMP_PEER_IDS.insert( ids[i] );
    }
  }

  if (vm.count("master-host")) {
    MASTER_ADDRESS = vm["master-host"].as<string>();
  }

  if (vm.count("host")) {
    vector<string> hosts =  vm["host"].as<vector<string>>();
    for(size_t i = 0; i < hosts.size(); i++) {
      COMP_PEER_HOSTS[i] = hosts[i];
    }
  }

  try {
    run_mpc();
  } catch (std::exception& e) {
    io.stop();
  }
}
