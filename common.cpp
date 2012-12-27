
#include "common.hpp"

int THREAD_COUNT = 4;
int TASK_COUNT = 1;

int mod( int64_t x, int m) {
    return (x%m + m)%m;
}
