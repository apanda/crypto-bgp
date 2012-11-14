#include <peer/peer.hpp>


LoggerPtr Peer::logger_(Logger::getLogger("all.peer"));


Peer::Peer(const short port) :
    counter_(0),
    server_(new RPCServer(io_service_, port, this)),
    lock_(cond_mutex_)
{

  barrier_mutex_.lock();

  tg_.add_thread( new boost::thread(bind(&io_service::run, &io_service_)) );
}



Peer::~Peer() {
  io_service_.stop();
  tg_.interrupt_all();
}



void Peer::publish(std::string key, int64_t value) {

  tbb::mutex::scoped_lock lock(__mutex);

  values_.insert(make_pair(key, value));
  counter_++;
  bool done = (counter_ == 3);

  LOG4CXX_TRACE(logger_,counter_);

  if (done) {
    counter_ = 0;

    barrier_mutex_.unlock();
    //cv_.notify_all();

    LOG4CXX_TRACE(logger_, "Unlocking mutex!");
  }

}



void Peer::print_values() {

  for (auto pair: values_) {
    LOG4CXX_INFO(logger_, "\t" << pair.first << ": " << pair.second );
  }

}
