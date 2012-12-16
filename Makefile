all:
	g++ -Wall \
  -DTBB_USE_DEBUG -shared-libgcc -g -O0 --std=c++11 \
	*.cpp net/*.cpp peer/*.cpp secret_sharing/*.cpp \
  bgp/bgp.cpp bgp/vertex.cpp bgp/edge.cpp \
  -I. -lboost_random -lboost_graph -lboost_program_options \
	`pkg-config --libs gsl` -ltbb -lboost_system -lboost_thread -llog4cxx \
	-o mpc
