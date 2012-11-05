all:
	g++ -DTBB_USE_DEBUG -shared-libgcc -g -O3 --std=c++11 *.cpp -lboost_random `pkg-config --libs gsl` -ltbb -lboost_system -lboost_thread -llog4cxx
