set(SRC_LIST calluserservice.cpp ../user.pb.cc)
# set(SRC_LIST callfriendservice.cpp ../friend.pb.cc)

add_executable(consumer ${SRC_LIST})

target_link_libraries(consumer ${LIB_DIR}/libmprpc.so ${LIB_DIR}/libReactorX.so ${LIB_DIR}/libThreadPool.so zookeeper_mt pthread protobuf)