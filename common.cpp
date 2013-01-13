
#include "common.hpp"

size_t THREAD_COUNT = 4;
size_t TASK_COUNT = 1;

size_t VERTEX_START = 0;
size_t VERTEX_END = 0;

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
