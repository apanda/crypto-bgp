# The name of our project is "HELLO". CMakeLists files in this project can
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and
# to the root binary directory of the project as ${HELLO_BINARY_DIR}.
cmake_minimum_required (VERSION 2.6)

SET(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wall -g")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) 

project (MPC)

# Recurse into the "Hello" and "Demo" subdirectories. This does not actually
# cause another cmake executable to run. The same process will walk through
# the project's entire directory structure.
add_subdirectory (peer)
add_subdirectory (net)
add_subdirectory (secret_sharing)
add_subdirectory (bgp)

add_executable (master master.cpp common.cpp)

target_link_libraries (master secret_sharing)
target_link_libraries (master peer net  bgp)
target_link_libraries (master boost_graph gsl gslcblas m tbb boost_system 
boost_thread boost_program_options log4cxx)

add_executable (mpc main.cpp common.cpp)

target_link_libraries (mpc peer net secret_sharing bgp boost_graph gsl 
gslcblas m tbb boost_system boost_thread boost_program_options log4cxx)

