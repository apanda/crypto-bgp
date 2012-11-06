all:
	g++ \
	-DTBB_USE_DEBUG -shared-libgcc -g -O0 --std=c++11 \
	*.cpp net/*.cpp peer/*.cpp secret_sharing/*.cpp \
	-I. -lboost_random \
	`pkg-config --libs gsl` -ltbb -lboost_system -lboost_thread -llog4cxx \
	-o mpc
