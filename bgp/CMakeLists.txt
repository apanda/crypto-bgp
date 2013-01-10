add_library (bgp SHARED bgp.cpp  edge.cpp  vertex.cpp) 
target_link_libraries (bgp boost_random)
