
#include "common.hpp"

int THREAD_COUNT = 4;
int TASK_COUNT = 1;

int mod( int64_t x, int m) {
    return (x%m + m)%m;
}

bool is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(),
        s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
}
