#include <peer/peer.hpp>

LoggerPtr Peer::logger_(Logger::getLogger("all.peer"));


Peer::Peer(const short port) :
    server_(new RPCServer(io_service_, port, this)),
    counter_(0),
    values_(new value_map_t)
{

  barrier_mutex_.lock();
  tg_.add_thread( new boost::thread(bind(&io_service::run, &io_service_)) );
}



Peer::~Peer() {
  io_service_.stop();
  tg_.interrupt_all();

  delete server_;
}



void Peer::publish(std::string key, int64_t value) {

  tbb::mutex::scoped_lock lock(__mutex);

  values_->insert(make_pair(key, value));
  counter_++;
  bool done = (counter_ == 3);

  //LOG4CXX_TRACE(logger_,counter_);

  if (done) {
    counter_ = 0;

    barrier_mutex_.unlock();
    //cv_.notify_all();

    LOG4CXX_TRACE(logger_, "Unlocking mutex!");
  }

}



void Peer::print_values() {

  for (auto pair: (*values_)) {
    LOG4CXX_DEBUG(logger_, "Value: " << pair.first << ": " << pair.second );
  }

}
