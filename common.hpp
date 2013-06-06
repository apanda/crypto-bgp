#ifndef COMMON_HPP_
#define COMMON_HPP_

#include <cstddef>

#include <iostream>
#include <cstdio>
#include <functional>
#include <numeric>
#include <string>
#include <vector>

#include <log4cxx/logger.h>
#include "log4cxx/basicconfigurator.h"

#include <boost/lexical_cast.hpp>
#include <boost/array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/phoenix/bind.hpp>
#include <boost/function.hpp>

#include <bgp/common.hpp>

const size_t COMP_PEER_NUM = 3;

extern size_t THREAD_COUNT;
extern size_t TASK_COUNT;

extern size_t VERTEX_START;
extern size_t VERTEX_END;

extern size_t MAX_BATCH;
extern size_t GRAPH_SIZE;

extern size_t DESTINATION_VERTEX;

using std::string;
using std::vector;
using std::set;
using std::map;
using std::pair;
using std::deque;

using boost::array;

extern string MASTER_ADDRESS;
extern string WHOAMI;

static set<int> COMP_PEER_IDS;
static array<string, COMP_PEER_NUM> COMP_PEER_HOSTS;

enum {
  msg_ = 256 + 8 + 8,
  cmd_ = sizeof(uint32_t),
  length_ = cmd_ + msg_
};

enum CMD_TYPE {
  MSG = 0,
  SYNC = 1,
  INIT = 2
};

typedef int64_t share_t;

const size_t SHARE_SIZE = sizeof(share_t);
const size_t SHARE_BIT_SIZE = SHARE_SIZE * 8;
const size_t MASTER_PORT = 65000;
const size_t START_PORT = 10000;

const int64_t PRIME = 2147483647;
const int64_t PRIME_EQ = 3;

#include <secret_sharing/secret.hpp>


using std::stringstream;

using log4cxx::Logger;
using log4cxx::LoggerPtr;

using boost::function;
using boost::shared_ptr;
using boost::shared_array;

using boost::lexical_cast;
using boost::phoenix::bind;

using boost::asio::ip::tcp;
using boost::asio::io_service;

typedef int64_t plaintext_t;
typedef string symbol_t;

class update_vertex_t {
public:

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version)
  {
    ar & vertex_;
    ar & next_hop_;
  }

  uint16_t vertex_;
  uint16_t next_hop_;
};

inline bool operator<(const update_vertex_t& lhs, const update_vertex_t& rhs)
{
  return lhs.vertex_ < rhs.vertex_;
}

struct sync_init {
  vector<update_vertex_t> nodes_;
  uint32_t id_;
  string hostname_;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
      ar & nodes_;
      ar & id_;
      ar & hostname_;
  }

};

struct sync_response {
  typedef map<uint32_t, map<uint32_t, string> > hostname_mappings_t;
  hostname_mappings_t hostname_mappings_;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
      ar & hostname_mappings_;

  }
};


int mod( int64_t x, int m);
bool is_number(const std::string& s);

#endif /* COMMON_H_ */
