/*
 * session.cpp
 *
 *  Created on: Nov 3, 2012
 *      Author: vjeko
 */

#include <net/session.hpp>



void Session::start()  {

  //socket_.set_option(tcp::no_delay(true));

  char* data = new char[length_];

  boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
        boost::bind(&Session::handle_read, this, data,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred)
  );
}



void Session::handle_preread(
    int* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {

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


void Session::handle_read(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {

   if (!error) {

     uint32_t& command =  *((uint32_t*) data);
     if(command == CMD_TYPE::MSG) {
       handle_msg(data, error, bytes_transferred);
     }


     boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
           boost::bind(&Session::handle_read, this, data,
               boost::asio::placeholders::error,
               boost::asio::placeholders::bytes_transferred)
     );

   } else {
     delete this;
   }

}



void Session::notify(string key,  int64_t value, vertex_t vertex) {

  char* data = new char[length_];
  char* msg = data + cmd_;

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


  write_impl(data, length_, socket_);

}


void Session::write_impl(char* data, size_t length, tcp::socket& socket) {

  boost::asio::async_write(socket,
      boost::asio::buffer(data, length),
      boost::bind(&Session::handle_write, this, data,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred)
  );

}



void Session::handle_write(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {}



tcp::socket& Session::socket()  {
  return socket_;
}
