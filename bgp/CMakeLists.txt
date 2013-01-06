add_library (bgp SHARED bgp.cpp  edge.cpp  vertex.cpp) 
target_link_libraries (bgp secret_sharing boost_random)
