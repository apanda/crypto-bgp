/*
 * rpc.cpp
 *
 *  Created on: Nov 3, 2012
 *      Author: vjeko
 */

#include <net/rpc_client.hpp>
#include <bgp/vertex.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>

LoggerPtr RPCClient::logger_(Logger::getLogger("all.peer.client"));

class Peer;


RPCClient::RPCClient(
    io_service& io,
    string hostname,
    int64_t port) :
        io_service_(io),
        buffer_queue_(1000),
        timer_(io),
        barrier_(new boost::barrier(COMP_PEER_NUM + 1)),
        socket_(io), resolver_(io), strand_(io) {

  const string service = lexical_cast<string>(port);

  LOG4CXX_TRACE(logger_, "Connecting to: " << hostname << ": " << service);

  tcp::resolver::query query(tcp::v4(), hostname, service);
  tcp::resolver::iterator iterator = resolver_.resolve(query);

  boost::asio::connect(socket_, iterator);
  socket_.set_option(tcp::no_delay(true));

  char* data = new char[length_];
  read_impl(data, length_, socket_);

  write_loop();
}



void RPCClient::read_impl(char* data, size_t length, tcp::socket& socket) {

  boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
        boost::bind(&RPCClient::handle_read, this, data,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred)
  );
}



void RPCClient::init(sync_init& si) {

  std::ostringstream archive_stream;
  boost::archive::binary_oarchive archive(archive_stream);
  archive << si;

  size_t real_length = sizeof(uint32_t)*3 + archive_stream.str().size();
  size_t length = real_length;

  if (length < length_) length = length_;

  char* data = new char[length];

  uint32_t& command = *((uint32_t*) data);
  uint32_t& size    = *((uint32_t*) (data + sizeof(uint32_t)));
  char* array       = data + sizeof(uint32_t)*3;

  memcpy(array, archive_stream.str().c_str(), archive_stream.str().size());

  command = CMD_TYPE::INIT;
  size = real_length;

  boost::asio::async_write(socket_,
      boost::asio::buffer(data, length),
      strand_.wrap(
      boost::bind(&RPCClient::handle_write, this, data,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred)));
}



void RPCClient::sync(vector<update_vertex_t>& nodes) {

  size_t real_length = sizeof(uint32_t) + sizeof(uint32_t) + nodes.size() * sizeof(update_vertex_t);
  size_t length = real_length;

  if (length < length_) length = length_;

  char* data = new char[length];

  uint32_t& command =  *((uint32_t*) data);
  uint32_t& size =  *((uint32_t*) (data + sizeof(uint32_t)));
  update_vertex_t* array = (update_vertex_t*) (data + sizeof(uint32_t)*2);

  command = CMD_TYPE::SYNC;
  size = real_length;

  for(size_t i = 0; i < nodes.size(); i++) {
    array[i] = nodes[i];
  }

  boost::unique_lock<boost::mutex> lock(m_);
  boost::asio::write(socket_, boost::asio::buffer(data, length));
  delete data;

  /*
  boost::asio::async_write(socket_,
      boost::asio::buffer(data, length),
      strand_.wrap(
      boost::bind(&RPCClient::handle_write, this, data,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred)));
*/
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


  new_write(data);


  /*
  boost::unique_lock<boost::mutex> lock(m_);
  boost::asio::write(socket_, boost::asio::buffer(data, length_));
  delete data;

  boost::asio::async_write(socket_,
      boost::asio::buffer(data, length_),
      boost::bind(&RPCClient::handle_write, this, data,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
            */
}


void RPCClient::new_write(char* data) {

  boost::lock_guard<boost::mutex> lock(zzz_);
  buffer_queue_.push(data);

};

void RPCClient::write_loop() {

  auto f = boost::bind(&RPCClient::write_loop, this);

  vector<char*> data_vec;
  char* data;
  /*
  if (buffer_queue_.try_pop(data)) {
    boost::unique_lock<boost::mutex> lock(m_);
    boost::asio::write(socket_, boost::asio::buffer(data, length_));
    delete data;
  }
*/

  int counter = 0;
  while (!buffer_queue_.empty()) {
    char* element;
    auto popped = buffer_queue_.pop(element);
    if (!popped) break;
    data_vec.push_back(element);
  }

  size_t size = data_vec.size();
  if (size) {

    char* new_data = new char[length_ * size];
    for(auto i = 0; i < size; i++) {
      memcpy(new_data + length_ * i, data_vec[i], length_);
    }

    boost::unique_lock<boost::mutex> lock(m_);
    boost::asio::write(socket_, boost::asio::buffer(new_data, length_*size));
  }

  timer_.expires_from_now(boost::posix_time::milliseconds(10));
  timer_.async_wait(f);
};



void RPCClient::handle_read(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred) {


  if (!error) {

    if (bytes_transferred == length_) {
      uint32_t& command =  *((uint32_t*) data);

      if (command == CMD_TYPE::SYNC) {

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


      } else if (command == CMD_TYPE::INIT) {

        uint32_t& size =  *((uint32_t*) (data + sizeof(uint32_t)));

        if (size > length_) {

          char* new_data = new char[size];
          memcpy(new_data, data, length_);

          boost::asio::async_read(socket_, boost::asio::buffer(new_data + length_, size - length_),
             boost::bind(&RPCClient::handle_init, this, new_data,
                 boost::asio::placeholders::error,
                 boost::asio::placeholders::bytes_transferred));

        } else {
          handle_init(data, error, bytes_transferred);
        }

        return;

      } else {
        throw std::runtime_error("invalid command");
      }

    }


  }

}



void RPCClient::handle_init(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {

  uint32_t& size    = *((uint32_t*) (data + sizeof(uint32_t) ));
  char* array                      = data + sizeof(uint32_t) * 3;
  const size_t object_size         = size - sizeof(uint32_t) * 3;


  struct sync_response sr;

  string object(array, object_size);
  std::stringstream ss;
  ss.write(array, object_size);
  boost::archive::binary_iarchive ia(ss);
  ia >> sr;

  hm_ = new sync_response::hostname_mappings_t(sr.hostname_mappings_);

  boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
        boost::bind(&RPCClient::handle_read, this, data,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));

  barrier_->wait();
}




void RPCClient::handle_sync(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {

  uint32_t& size =  *((uint32_t*) (data + sizeof(uint32_t)));

  update_vertex_t* array = (update_vertex_t*) (data + sizeof(uint32_t) * 2);
  const size_t num = (size - sizeof(uint32_t) * 2) / sizeof(update_vertex_t);

  array_ = array;
  size_ = num;

  boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
        boost::bind(&RPCClient::handle_read, this, data,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));

  barrier_->wait();
}



void RPCClient::handle_write(
      char* data,
      const boost::system::error_code& error,
      size_t bytes_transferred) {

  delete data;
}
