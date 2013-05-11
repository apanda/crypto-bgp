cmake_minimum_required (VERSION 2.6)

include_directories(${INCLUDE_PATH})
link_directories(${LIB_PATH})

project (MPC)

SET(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wall -g")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) 

add_subdirectory (peer)
add_subdirectory (net)
add_subdirectory (secret_sharing)
add_subdirectory (bgp)

add_executable (mpc main.cpp common.cpp)

target_link_libraries (mpc peer net secret_sharing bgp boost_graph gsl 
gslcblas m tbb boost_system boost_thread boost_program_options log4cxx)

