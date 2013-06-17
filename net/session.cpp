/*
 * session.cpp
 *
 *  Created on: Nov 3, 2012
 *      Author: vjeko
 */

#include <net/session.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>

#include <fstream>



void Session::start()  {

  socket_.set_option(tcp::no_delay(true));

  char* data = new char[buf_length_];

  socket_.async_read_some(boost::asio::buffer(data, buf_length_),
        boost::bind(&Session::handle_read, this, data,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
}



void Session::handle_msg(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {

  char* msg = data + cmd_;

  int64_t value;
  vertex_t vertex;

  memcpy(
      &vertex,
      msg + (msg_ - sizeof(int64_t) - sizeof(vertex_t)),
      sizeof(vertex_t));

  memcpy(
      &value,
      msg + (msg_ - sizeof(int64_t)),
      sizeof(int64_t));

  peer_->publish(msg, value, vertex);

}



void Session::handle_init(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {

  uint32_t& size    = *((uint32_t*) (data + sizeof(uint32_t) ));
  char* array                      = data + sizeof(uint32_t) * 3;
  const size_t object_size         = size - sizeof(uint32_t) * 3;


  sync_init si;

  string object(array, object_size);
  std::stringstream ss;
  ss.write(array, object_size);
  boost::archive::binary_iarchive ia(ss);
  ia >> si;

  peer_->publish(this, si);

}



void Session::handle_sync(
    char* data,
    const boost::system::error_code& error,
    size_t size) {

  char* new_data = new char[length_];

  boost::asio::async_read(socket_, boost::asio::buffer(new_data, length_),
        boost::bind(&Session::handle_read, this, new_data,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred)
      );

  std::vector<update_vertex_t> nodes;

  update_vertex_t* array = (update_vertex_t*) (data + sizeof(uint32_t) * 2);
  const size_t num = (size - sizeof(uint32_t) * 2) / sizeof(update_vertex_t);
  for (size_t i = 0; i < num; i++) {
    nodes.push_back(array[i]);
  }

  peer_->publish(this, nodes);
}



void Session::handle_read(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {

   if (error) {
     delete this;
   }

   if (bytes_transferred < length_) {
     BOOST_ASSERT_MSG(false, "bytes_transferred < length_");
   }

   auto offset = 0;
   do {

     data = data + offset;

     const uint32_t& command =  *((uint32_t*) data);
     const uint32_t& size =  *((uint32_t*) (data + sizeof(uint32_t)));

     if(command == CMD_TYPE::MSG) {
       handle_msg(data, error, size);

     } else if (command == CMD_TYPE::SYNC) {
       handle_sync(data, error, size);

     } else if (command == CMD_TYPE::INIT) {
         handle_init(data, error, size);
     } else {
       LOG4CXX_FATAL(peer_->logger_, "Unknown command: " << command);
       throw std::runtime_error("invalid command");
     }

     offset += size;

   } while (offset < bytes_transferred);

   socket_.async_read_some(boost::asio::buffer(data, buf_length_),
         boost::bind(&Session::handle_read, this, data,
             boost::asio::placeholders::error,
             boost::asio::placeholders::bytes_transferred));

}



void Session::sync_response(struct sync_response& sr){

  std::ostringstream archive_stream;
  boost::archive::binary_oarchive archive(archive_stream);
  archive << sr;

  uint32_t real_length = sizeof(uint32_t)*3 + archive_stream.str().size();
  uint32_t length = real_length;

  if (length < length_) length = length_;

  char* data = new char[length];

  uint32_t& command = *((uint32_t*) data);
  uint32_t& size    = *((uint32_t*) (data + sizeof(uint32_t)));
  char* array       =                data + sizeof(uint32_t)*3;

  size = real_length;
  memcpy(array, archive_stream.str().c_str(), archive_stream.str().size());

  command = CMD_TYPE::INIT;

  write_impl(data, length, socket_);
}



pair<char*, size_t> Session::contruct_notification(vector<update_vertex_t>& nodes) {

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

  return std::make_pair(data, length);
}



void Session::notify(vector<vertex_t>& nodes) {

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

  write_impl(data, length, socket_);
}



void Session::write_impl(char* data, size_t length, tcp::socket& socket) {
  boost::asio::async_write(socket,
      boost::asio::buffer(data, length),
      boost::bind(&Session::handle_write, this, data,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
}


void Session::read_impl(char* data, size_t length, tcp::socket& socket) {
}



void Session::handle_write(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {}



tcp::socket& Session::socket()  {
  return socket_;
}
