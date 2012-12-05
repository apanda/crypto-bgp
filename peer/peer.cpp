#include <peer/peer.hpp>

LoggerPtr Peer::logger_(Logger::getLogger("all.peer"));


Peer::Peer(const short port) :
    server_(new RPCServer(io_service_, port, this)),
    counter_(0),
    values_(new value_map_t) {

  barrier_mutex_.lock();
  tg_.add_thread( new boost::thread(bind(&io_service::run, &io_service_)) );
}



Peer::~Peer() {
  io_service_.stop();
  tg_.interrupt_all();

  delete server_;
}



void Peer::publish(std::string key, int64_t value, vertex_t vertex) {



  value_map_t& vlm = vertex_value_map_[vertex];

  //auto iter = vlm.insert(make_pair(key, value));
  //if (!iter.second) {
    //vlm.unsafe_erase(key);
    vlm[key] = value;
    //iter = vlm.insert(make_pair(key, value));
    //BOOST_ASSERT(iter.second);
  //}

  LOG4CXX_INFO(logger_, " Received value: " << key << ": " << value << " (" << vertex << ")");

  tbb::mutex::scoped_lock lock(__mutex);


  int& count = couter_map_[vertex];


  count++;
  LOG4CXX_TRACE(logger_, "Counter: " << count);

  if (count == 3) {

    count = 0;
    barrier_mutex_.unlock();
    //cv_.notify_all();
    //LOG4CXX_TRACE(logger_, "Unlocking mutex!");
  }

}



void Peer::print_values() {

  for (auto pair: (*values_)) {
    LOG4CXX_DEBUG(logger_, "Value: " << pair.first << ": " << pair.second );
  }

}
