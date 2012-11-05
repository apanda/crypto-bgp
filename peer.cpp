#include "peer.hpp"


LoggerPtr Peer::logger_(Logger::getLogger("Peer"));


Peer::Peer(const short port) :
    lock_(mutex_),
    server_(new RPCServer(io_service_, port, this))
{
  counter_ = 0;

  new boost::thread(bind(&io_service::run, &io_service_));
}



void Peer::publish(std::string key, int value) {

  values_[key] = value;
  bool done = (++counter_ == 3);

  if (done) {
    condition_.notify_all();
    counter_ = 0;
  }

}



void Peer::print_values() {

  for (auto pair: values_) {
    LOG4CXX_INFO(logger_, "\t" << pair.first << ": " << pair.second );
  }

}
