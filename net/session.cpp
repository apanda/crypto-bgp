/*
 * session.cpp
 *
 *  Created on: Nov 3, 2012
 *      Author: vjeko
 */

#include <net/session.hpp>



void Session::start()  {

  char* data = new char[length_];

  boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
      boost::bind(&Session::handle_read, this, data,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
}



void Session::handle_read(
    char* data,
    const boost::system::error_code& error,
    size_t bytes_transferred) {

   if (!error) {

     int64_t value;

     memcpy(
         &value,
         data + (length_ - sizeof(int64_t)),
         sizeof(int64_t));


     LOG4CXX_TRACE(peer_->logger_, "Received value: " << data << ": " << value);
     peer_->publish(data, value);
   }

   //delete data;
   //char* new_data = new char[length_];

   boost::asio::async_read(socket_, boost::asio::buffer(data, length_),
       boost::bind(&Session::handle_read, this, data,
           boost::asio::placeholders::error,
           boost::asio::placeholders::bytes_transferred));
}



tcp::socket& Session::socket()  {
  return socket_;
}
