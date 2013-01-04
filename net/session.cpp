/*
 * session.cpp
 *
 *  Created on: Nov 3, 2012
 *      Author: vjeko
 */

#include <net/session.hpp>



void Session::start()  {

  char* data = new char[length_];

  //socket_.set_option(tcp::no_delay(true));

  boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
      //strand_.wrap(
        boost::bind(&Session::handle_read, this, data,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred)
      //)
  );
}



void Session::handle_read(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {

   if (!error) {

     int64_t value;
     vertex_t vertex;

     memcpy(
         &vertex,
         data + (length_  - sizeof(int64_t) - sizeof(vertex_t)),
         sizeof(vertex_t));

     memcpy(
         &value,
         data + (length_ - sizeof(int64_t)),
         sizeof(int64_t));

     peer_->publish(data, value, vertex);
   }

   boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
         boost::bind(&Session::handle_read, this, data,
             boost::asio::placeholders::error,
             boost::asio::placeholders::bytes_transferred)
   );
}



void Session::notify(string key,  int64_t value, vertex_t vertex) {

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
