#include <iostream>
#include "test.pb.h"
using namespace fixbug;

int main(){
    // 封装了login请求对象的数据
    LoginRequest req;
    req.set_name("zhang san");
    req.set_pwd("123456");
    // 对象数据序列化 -> char*;
    std::string send_str;
    const char* str;
    if(req.SerializeToString(&send_str)){
        str = send_str.c_str();
        std::cout << std::hex << send_str.c_str() << std::endl;
    }

    // 反序列化 从send_str反序列化一个login请求
    LoginRequest reqB;
    if(reqB.ParseFromString(send_str)){
        std::cout << reqB.name() << std::endl;
        std::cout << reqB.pwd() << std::endl;
    }

    return 0;
}

