#pragma once
#include <string>
#include <functional>
#include <TcpServer.h>
#include <EventLoop.h>
#include <InetAddress.h>
#include <memory>
#include "google/protobuf/service.h"
#include "mprpcapplication.h"
#include <google/protobuf/descriptor.h>
namespace rpc{
// 框架提供的专门发布rpc服务的网络对象类
class MpRpcProvider{
public:
    // 这里是框架给外部使用的，可以发布rpc方法的函数接口
    void NotifyService(google::protobuf::Service *service);

    // 启动rpc服务节点，开始提供rpc远程远程网络调用服务
    void Run();

private:
    // 服务类型信息
    struct ServiceInfo{
        google::protobuf::Service* m_service; // 保存服务对象
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> m_methodMap;
    };
    // 存储注册成功的服务对象和其服务方法的所有信息
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;

    // 组合EventLoop
    rx::EventLoop m_eventLoop;

    // 新的socket连接回调
    void OnConnection(const rx::TcpConnectionPtr&);
    // 已建立连接用户的读写事件回调
    void OnMessage(const rx::TcpConnectionPtr&, rx::Buffer*, Timestamp);

    // closure的回调操作，用于序列化rpc的响应和网络发送
    void SendRpcResponse(const rx::TcpConnectionPtr&, google::protobuf::Message*);

};
};