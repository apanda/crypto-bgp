add_library (net SHARED session.cpp rpc_server.cpp rpc_client.cpp) 
target_link_libraries (net boost_serialization boost_chrono)
