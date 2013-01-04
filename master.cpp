
#include <boost/program_options.hpp>
#include <secret_sharing/secret.hpp>

#include <peer/master_peer.hpp>

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

  MasterPeer m(1, io);

  worker_threads.add_thread( new boost::thread(bind(&io_service::run, &io)) );
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
