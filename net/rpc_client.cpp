/*
 * rpc.cpp
 *
 *  Created on: Nov 3, 2012
 *      Author: vjeko
 */

#include <net/rpc_client.hpp>
#include <bgp/vertex.hpp>

LoggerPtr RPCClient::logger_(Logger::getLogger("all.peer.client"));


class Peer;

RPCClient::RPCClient(io_service& io_service, string hostname,  int64_t port, Peer*) :
  socket_(io_service), resolver_(io_service), strand_(io_service) {

  const string service = lexical_cast<string>(port);

  LOG4CXX_TRACE(logger_, "Connecting to: " << service);

  tcp::resolver::query query(tcp::v4(), hostname, service);
  tcp::resolver::iterator iterator = resolver_.resolve(query);

  boost::asio::connect(socket_, iterator);
  //socket_.set_option(tcp::no_delay(true));
}



void RPCClient::publish(string key,  int64_t value, vertex_t vertex) {

  char* data = new char[length_];

  BOOST_ASSERT(key.length() < (length_ - sizeof(vertex_t) - sizeof(int64_t)));
  strcpy(
      data,
      key.c_str());

  memcpy(
      data + (length_ - sizeof(vertex_t) - sizeof(int64_t)),
      &vertex,
      sizeof(vertex_t));

  memcpy(
      data + (length_ - sizeof(int64_t)),
      &value,
      sizeof(int64_t));


  LOG4CXX_TRACE(logger_, "Sending value: " << key << ": " << value << " (" << vertex << ")");

  boost::asio::async_write(socket_,
      boost::asio::buffer(data, length_),
      //strand_.wrap(
      boost::bind(&RPCClient::handle_write, this, data,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred)
    //)
  );
}



void RPCClient::handle_write(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred) {

  delete data;

}
