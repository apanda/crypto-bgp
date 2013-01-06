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
void Peer::publish(vector<vertex_t>& nodes) {

}



void Peer::subscribe(std::string key, int64_t value, vertex_t v) {

}



void Peer::print_values() {

}
