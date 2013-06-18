
#include "common.hpp"

size_t THREAD_COUNT = 1;
size_t TIMER = 100;

size_t MAX_BATCH = 30;
size_t GRAPH_SIZE = 101;

size_t DESTINATION_VERTEX = 1;

string MASTER_ADDRESS = "localhost";
string WHOAMI = "";

int mod( int64_t x, int m) {
    return (x%m + m)%m;
}

void hexdump(void *ptr, int buflen) {
  unsigned char *buf = (unsigned char*)ptr;
  int i, j;
  for (i=0; i<buflen; i+=16) {
    printf("%06x: ", i);
    for (j=0; j<16; j++)
      if (i+j < buflen)
        printf("%02x ", buf[i+j]);
      else
        printf("   ");
    printf(" ");
    for (j=0; j<16; j++)
      if (i+j < buflen)
        printf("%c", isprint(buf[i+j]) ? buf[i+j] : '.');
    printf("\n");
  }
}


bool is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(),
        s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
}
