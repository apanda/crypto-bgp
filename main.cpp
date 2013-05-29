
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

  graph_t graph(GRAPH_SIZE);
  BGPProcess::load_graph("scripts/dot.dot", graph);

  MasterPeer mp(GRAPH_SIZE, io);

  boost::thread_group worker_threads;
  for (size_t i = 0; i < THREAD_COUNT - 1; i++) {
    worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  }

  io.run();
}



std::chrono::high_resolution_clock::time_point t1;

bool measure_time() {

  typedef std::chrono::high_resolution_clock clock_t;

  const auto t2 = clock_t::now();
  const auto duration = duration_cast<milliseconds>(t2 - t1).count();

  LOG4CXX_INFO(mainLogger, "The execution took " << duration << " ms.");

  io.stop();

  return true;
}


void run_mpc() {

  typedef std::chrono::high_resolution_clock clock_t;

  array<shared_ptr<comp_peer_t>, COMP_PEER_NUM> comp_peer_seq;
  shared_ptr<InputPeer> input_peer(new InputPeer(io));

  boost::thread_group worker_threads;

  typedef function<bool()> functor_t;

  boost::barrier* b = new boost::barrier(COMP_PEER_IDS.size() + 1);
  functor_t f = boost::bind(&boost::barrier::wait, b);

  for (size_t i = 0; i < THREAD_COUNT; i++) {
    worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
  }


  CompPeer_factory factory;
  comp_peer_seq = factory.generate<COMP_PEER_NUM>(input_peer, io);


  BGPProcess bgp("scripts/dot.dot", NULL, io);
  graph_t& input_graph = bgp.graph_;

  shared_ptr<RPCClient> master( new RPCClient(io, MASTER_ADDRESS, MASTER_PORT) );
  bgp.master_ = master;

  LOG4CXX_INFO(mainLogger, "Number of vertices in the graph: " << GRAPH_SIZE);
  LOG4CXX_INFO(mainLogger, "Starting listeners...");

  sync_init init;
  init.nodes_ = input_peer->start_listeners(comp_peer_seq, input_graph);
  init.hostname_ = WHOAMI;

  master->barrier_ = new boost::barrier(2);

  for(auto& cp: comp_peer_seq) {
    if (COMP_PEER_IDS.find(cp->id_) == COMP_PEER_IDS.end()) continue;

    cp->bgp_->master_ = master;
    init.id_ = cp->id_;

    LOG4CXX_INFO(mainLogger, "master->init(init))");
    master->init(init);
  }

  master->barrier_ ->wait();

  input_peer->start_clients(comp_peer_seq, input_graph, master->hm_);

  LOG4CXX_INFO(mainLogger, "All clients have been started.");

  vector<update_vertex_t> nodes;
  for(auto& cp: comp_peer_seq) {
    if (COMP_PEER_IDS.find(cp->id_) == COMP_PEER_IDS.end()) continue;
    LOG4CXX_INFO(mainLogger, "master->sync(nodes)");
    master->sync(nodes);
  }

  master->barrier_->wait();

  LOG4CXX_INFO(mainLogger, "Master has raised the barrier.");

  sleep(1);

  master->barrier_ = new boost::barrier(COMP_PEER_IDS.size() + 1);

  functor_t f2 = boost::bind(measure_time);

  for (auto& cp : comp_peer_seq) {
    if (COMP_PEER_IDS.find(cp->id_) == COMP_PEER_IDS.end()) continue;
    BGPProcess* bgp = cp->bgp_.get();
    io.post(boost::phoenix::bind(&BGPProcess::start_callback, bgp, f2));
  }

  t1 = clock_t::now();
  io.run();
}



int main(int argc, char *argv[]) {

  namespace po = boost::program_options;

  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ("master", "start the master peer")
      ("threads", po::value<size_t>(), "total number of threads")
      ("master-host", po::value<string>(), "master address")
      ("whoami", po::value<string>(), "out address")
      ("start", po::value<size_t>(), "staring vertex")
      ("end", po::value<size_t>(), "ending vertex")
      ("id", po::value<vector<int>>()->multitoken(), "computational peer id")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  log4cxx::PropertyConfigurator::configure("apache.conf");

  if (vm.count("help")) {
    std::cout << desc << std::endl;;
    return 1;
  }


  if (vm.count("threads")) {
    THREAD_COUNT = vm["threads"].as<size_t>();
  }

  if (vm.count("start")) {
    VERTEX_START = vm["start"].as<size_t>();
  }

  if (vm.count("end")) {
    VERTEX_END = vm["end"].as<size_t>();
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

  if (vm.count("whoami")) {
    WHOAMI = vm["whoami"].as<string>();
  }

  GRAPH_SIZE = BGPProcess::get_graph_size("scripts/dot.dot") + 1;

  if (vm.count("master")) {
    run_master();
    return 0;
  }

  try {
    run_mpc();
  } catch (std::exception& e) {
    std::cout << "Exception: " << e.what() << std::endl;
    io.stop();
  }
}
