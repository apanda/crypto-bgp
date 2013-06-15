
#include "common.hpp"

size_t THREAD_COUNT = 1;
size_t TIMER = 10;

size_t MAX_BATCH = 30;
size_t GRAPH_SIZE = 101;

size_t DESTINATION_VERTEX = 1;

string MASTER_ADDRESS = "localhost";
string WHOAMI = "";

int mod( int64_t x, int m) {
    return (x%m + m)%m;
}

bool is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(),
        s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
}
