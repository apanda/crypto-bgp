#include <peer/peer.hpp>


LoggerPtr Peer::logger_(Logger::getLogger("Peer"));


Peer::Peer(const short port) :
    server_(new RPCServer(io_service_, port, this))
{

  counter_ = 0;
  barrier_mutex_.lock();

  tg_.add_thread( new boost::thread(bind(&io_service::run, &io_service_)) );
}



Peer::~Peer() {
  io_service_.stop();
  tg_.interrupt_all();
}



void Peer::publish(std::string key, int64_t value) {

  values_.insert(make_pair(key, value));
  bool done = (++counter_ == 3);

  __mutex.lock();
  if (done) {
    counter_ = 0;
    barrier_mutex_.unlock();
  }
  __mutex.unlock();

}



void Peer::print_values() {

  for (auto pair: values_) {
    LOG4CXX_INFO(logger_, "\t" << pair.first << ": " << pair.second );
  }

}
