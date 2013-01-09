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


RPCClient::RPCClient(
    io_service& io_service,
    string hostname,
    int64_t port) :
        barrier_(new boost::barrier(COMP_PEER_NUM + 1)),
        socket_(io_service), resolver_(io_service), strand_(io_service) {

  const string service = lexical_cast<string>(port);

  LOG4CXX_TRACE(logger_, "Connecting to: " << service);

  tcp::resolver::query query(tcp::v4(), hostname, service);
  tcp::resolver::iterator iterator = resolver_.resolve(query);

  boost::asio::connect(socket_, iterator);
  socket_.set_option(tcp::no_delay(true));

  char* data = new char[length_];
  read_impl(data, length_, socket_);
}



void RPCClient::read_impl(char* data, size_t length, tcp::socket& socket) {

  boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
        boost::bind(&RPCClient::handle_read, this, data,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred)
  );

}



void RPCClient::sync(vector<vertex_t>& nodes) {

  size_t real_length = sizeof(uint32_t) + sizeof(uint32_t) + nodes.size() * sizeof(uint16_t);
  size_t length = real_length;

  if (length < length_) length = length_;

  char* data = new char[length];

  uint32_t& command =  *((uint32_t*) data);
  uint32_t& size =  *((uint32_t*) (data + sizeof(uint32_t)));
  uint16_t* array = (uint16_t*) (data + sizeof(uint32_t)*2);

  command = CMD_TYPE::SYNC;
  size = real_length;

  for(size_t i = 0; i < nodes.size(); i++) {
    array[i] = nodes[i];
  }

  boost::asio::async_write(socket_,
      boost::asio::buffer(data, length),
      strand_.wrap(
      boost::bind(&RPCClient::handle_write, this, data,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred)
      )
  );

}



void RPCClient::publish(string key,  int64_t value, vertex_t vertex) {

  char* data = new char[length_];
  char* msg = data + cmd_;

  uint32_t& command =  *((uint32_t*) data);
  command = CMD_TYPE::MSG;

  BOOST_ASSERT(key.length() < (msg_ - sizeof(vertex_t) - sizeof(int64_t)));

  strcpy(
      msg,
      key.c_str());

  memcpy(
      msg + (msg_ - sizeof(vertex_t) - sizeof(int64_t)),
      &vertex,
      sizeof(vertex_t));

  memcpy(
      msg + (msg_ - sizeof(int64_t)),
      &value,
      sizeof(int64_t));


  LOG4CXX_TRACE(logger_, "Sending value: " << key << ": " << value << " (" << vertex << ")");

  boost::asio::async_write(socket_,
      boost::asio::buffer(data, length_),
      boost::bind(&RPCClient::handle_write, this, data,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));

}



void RPCClient::handle_read(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred) {

  printf("Handle read.\n");

  if (!error) {

    if (bytes_transferred == length_) {

      uint32_t& command =  *((uint32_t*) data);

      if (command == CMD_TYPE::SYNC) {
        printf("handle_sync \n");

        uint32_t& size =  *((uint32_t*) (data + sizeof(uint32_t)));

        if (size > length_) {

          char* new_data = new char[size];
          memcpy(new_data, data, length_);

          boost::asio::async_read(socket_, boost::asio::buffer(new_data + length_, size - length_),
             boost::bind(&RPCClient::handle_sync, this, new_data,
                 boost::asio::placeholders::error,
                 boost::asio::placeholders::bytes_transferred));

        } else {
          handle_sync(data, error, bytes_transferred);
        }

      } else {
        throw std::runtime_error("invalid command");
      }

      boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
            boost::bind(&RPCClient::handle_read, this, data,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }


  }

}



void RPCClient::handle_sync(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {

  uint32_t& size =  *((uint32_t*) (data + sizeof(uint32_t)));

  uint16_t* array = (uint16_t*) (data + sizeof(uint32_t) * 2);
  const size_t num = (size - sizeof(uint32_t) * 2) / sizeof(uint16_t);

  array_ = array;
  size_ = num;

  barrier_->wait();
}



void RPCClient::handle_write(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred) {

  delete data;
}
