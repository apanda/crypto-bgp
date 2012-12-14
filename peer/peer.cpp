#include <peer/peer.hpp>

LoggerPtr Peer::logger_(Logger::getLogger("all.peer"));


Peer::Peer(const short port, io_service& io) :
    io_service_(io),
    counter_(0) {

  for(int i = 0; i < 500; i++) {
    mutex_map_[i] = shared_ptr<mutex_t>(new mutex_t);
  }

  for(int i = 0; i < 500; i++) {
    cv_map_[i] = shared_ptr<condition_variable_t>(new condition_variable_t);
  }

}



Peer::~Peer() {
  io_service_.stop();
  tg_.interrupt_all();
}



void Peer::publish(std::string key, int64_t value, vertex_t vertex) {


  string rkey = key.substr(0, key.size() - 2);
  //std::cout << "publish LOCK -> " << rkey << std::endl;

  auto p =mutex_map_2[vertex].insert(
      make_pair(rkey, shared_ptr<mutex_t>(new mutex_t))
      );
  mutex_t& m = *(p.first->second);
  lock_t lock(m);

  LOG4CXX_TRACE(logger_, " Acquired lock... " << vertex << ": " << rkey);

  auto cv_p = cv_map_2[vertex].insert(
      make_pair(rkey, shared_ptr<condition_variable_t>(new condition_variable_t))
      );
  condition_variable_t& cv = *(cv_p.first->second);

  int& counter = couter_map_2[vertex][rkey];

  //int& count = couter_map_[vertex];

  while (counter >= 3) {

    LOG4CXX_INFO(logger_, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");

    cv.notify_all();
    cv.wait(lock);
  }

  value_map_t& vlm = vertex_value_map_[vertex];
  vlm[key] = value;

  counter++;

  LOG4CXX_TRACE(logger_, "Counter... (" << vertex << "): " << rkey << " " << counter);
  cv.notify_all();

  LOG4CXX_TRACE(logger_, " Received value: " << key << ": " << value << " (" << vertex << ")");
}



void Peer::print_values() {

}
