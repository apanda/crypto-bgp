/*
 * rpc_server.cpp
 *
 *  Created on: Nov 4, 2012
 *      Author: vjeko
 */

#include <net/rpc_server.hpp>

RPCServer::RPCServer(
    boost::asio::io_service& io_service,
    const uint16_t port,
    Peer* peer)

: io_service_(io_service),
  acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
  peer_(peer),
  strand_(io_service) {


  LOG4CXX_TRACE(peer_->logger_, "Listening on port: " << port );
  start_accept();

}



RPCServer::~RPCServer() {};



void RPCServer::start_accept() {

  Session* new_session = new Session(io_service_, strand_, peer_);

  acceptor_.async_accept(new_session->socket(),
      boost::bind(&RPCServer::handle_accept, this, new_session,
        boost::asio::placeholders::error)
  );
}



void RPCServer::handle_accept(
    Session* new_session,
    const boost::system::error_code& error) {

  if (!error) {
    sessions_.push_back(new_session);
    new_session->start();
  } else {
    LOG4CXX_ERROR(peer_->logger_, error.message());
    delete new_session;
  }

  start_accept();
}
