set(SRC_LIST userservice.cpp ../user.pb.cc)
# set(SRC_LIST friendservice.cpp  ../friend.pb.cc)
add_executable(provider ${SRC_LIST})

target_link_libraries(provider ${LIB_DIR}/libmprpc.so ${LIB_DIR}/libReactorX.so ${LIB_DIR}/libThreadPool.so zookeeper_mt pthread protobuf)