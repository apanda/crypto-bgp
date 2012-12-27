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

extern int THREAD_COUNT;
extern int TASK_COUNT;

typedef int64_t share_t;

const size_t SHARE_SIZE = sizeof(share_t);
const size_t SHARE_BIT_SIZE = SHARE_SIZE * 8;

const size_t COMP_PEER_NUM = 3;
const int64_t PRIME = 2147483647;

#include <secret_sharing/secret.hpp>

using std::string;
using std::vector;
using std::stringstream;

using log4cxx::Logger;
using log4cxx::LoggerPtr;

using boost::function;
using boost::array;
using boost::shared_ptr;
using boost::shared_array;

using boost::lexical_cast;
using boost::phoenix::bind;

using boost::asio::ip::tcp;
using boost::asio::io_service;

typedef int64_t plaintext_t;
typedef string symbol_t;


int mod( int64_t x, int m);

#endif /* COMMON_H_ */
