#include <peer/peer.hpp>
#include <bgp/bgp.hpp>

LoggerPtr Peer::logger_(Logger::getLogger("all.peer"));


Peer::Peer(io_service& io) :
    io_service_(io),
    counter_(0) {
}



Peer::~Peer() {
  io_service_.stop();
  tg_.interrupt_all();
}



void Peer::publish(std::string key, int64_t value, vertex_t v) {}

void Peer::publish(Session* session, sync_init si) {
  throw std::runtime_error("not suppose to be called!");
}

void Peer::publish(Session* session, vector<update_vertex_t>& nodes, size_t id) {
  throw std::runtime_error("not suppose to be called!");
}



void Peer::subscribe(std::string key, int64_t value, vertex_t v) {

  throw std::runtime_error("not suppose to be callled!");
}



void Peer::print_values() {

}
