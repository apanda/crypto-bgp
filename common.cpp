
#include "common.hpp"

int mod( int64_t x, int m) {
    return (x%m + m)%m;
}
