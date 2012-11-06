/*
 * rpc_server.hpp
 *
 *  Created on: Nov 4, 2012
 *      Author: vjeko
 */

#ifndef RPC_SERVER_HPP_
#define RPC_SERVER_HPP_

#include <common.hpp>
#include <net/session.hpp>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

class Peer;
class Session;

class RPCServer {

public:
  RPCServer(
      boost::asio::io_service& io_service,
      const uint16_t port,
      Peer* peer);

private:

  void start_accept();

  void handle_accept(
      Session* new_session,
      const boost::system::error_code& error);

  io_service& io_service_;
  tcp::acceptor acceptor_;
  Peer* peer_;
};

#endif /* RPC_SERVER_HPP_ */
