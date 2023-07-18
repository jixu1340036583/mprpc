#include <iostream>
#include <thread>
#include <future>
#include "mprpcapplication.h"
#include "user.pb.h"
#include "Logger.h"
int main(int argc, char** argv){
    // 整个程序启动以后，想使用mprpc框架来享受rpc服务调用，一定需要先调用框架来初始化函数（只初始化一次）
    rpc::MprpcApplication::Init(argc, argv);
    // 演示调用远程发布的rpc方法Login
    fixbug::UserServiceRpc_Stub stub(new rpc::MprpcChannel());
    // rpc方法的请求参数
    fixbug::LoginRequest request;
    request.set_name("zhang san");
    request.set_pwd("123456");
    // rpc方法的响应
    fixbug::LoginResponse response;
    // 发起rpc方法的调用，同步的rpc调用过程 MprpcChannel::Callmethod

    rpc::MprpcController controller;
    // stub.Login(&controller, &request, &response, nullptr); // RpcChannel->RpcChannel::callMethod 集中来做所有rpc方法调用参数序列化和网络发送
    auto res = std::async(std::launch::async, &fixbug::UserServiceRpc_Stub::Login, &stub, &controller, &request, &response, nullptr);
    res.wait();
    // 一次rpc调用完成，读调用的结果
    if(controller.Failed()){ // rpc调用出错了
        std::cout << controller.ErrorText() << std::endl;
    }else{
        if(response.result().errcode() == 0){
            std::cout << "rpc login response success :" << response.success() << std::endl;
        }
        else{
            std::cout << "rpc login response error:" << response.result().errmsg() << std::endl;
        }
    }
    return 0;
}