#include "mprpcapplication.h"
#include "mprpcchannel.h"
#include "rpcheader.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "zookeeperutil.h"
#include "Logger.h"
namespace rpc{
/*
header_size + service_name method_name args_size + args
*/
// 所有通过stub代理对象调用rpc方法，都走到这里了,统一做rpc方法调用的数据序列化和网络发送和网络发送
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                          google::protobuf::Message* response, google::protobuf::Closure* done){

    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name();
    std::string method_name = method->name();

    // 获取参数的序列化字符串长度 args_size
    std::string args_str;
    uint32_t args_size = 0;
    if(request->SerializeToString(&args_str)){
        args_size = args_str.size();
    }else{
        controller->SetFailed("serialize request error!");
        return;
    }

    // 定义rpc的请求header
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if(rpcHeader.SerializeToString(&rpc_header_str)){
        header_size = rpc_header_str.size();
    }

    // 组织待发送rpc请求的字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4));
    send_rpc_str += rpc_header_str;
    send_rpc_str += args_str;

    LOG_DEBUG("=======待发送的rpcheader==========");
    LOG_DEBUG("header_size: %d", header_size);
    LOG_DEBUG("rpc_header_str: %s", rpc_header_str.c_str());
    LOG_DEBUG("service_name: %s", service_name.c_str());
    LOG_DEBUG("method_name: %s", method_name.c_str());
    LOG_DEBUG("args_str: %s", args_str.c_str());
    LOG_DEBUG("=================================");

    // 因为是客户端调用，不涉及高并发，仅使用TCP编程即可，完成rpc方法的远程调用
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(clientfd == -1){
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno: %d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 读取配置文件rpcserver的信息
    // std::string ip = MprpcApplication::GetConfig().Load("rpcserverip");
    // uint16_t port = atoi(MprpcApplication::GetConfig().Load("rpcserverport").c_str());
    ZkClient zkCli;
    zkCli.Start();
    std::string service_root_path = "/" + service_name + "_list";

    // 判断根节点是否存在
    int exists = zoo_exists(zkCli.getZandle(), service_root_path.c_str() , 0, nullptr);
    if (exists == ZOK) 
        LOG_INFO("Root node %s exists!", service_root_path.c_str());
    else{
        char errtxt[512] = {0};
        sprintf(errtxt, "Failed to check root node: %s !", service_root_path.c_str());
        controller->SetFailed(errtxt);
        return;
    }
    // 获取当前方法所在的所有服务节点
    struct String_vector children;
    int numChildren = 0;
    int ret = zoo_get_children(zkCli.getZandle(), service_root_path.c_str(), 0, &children);
    if (ret == ZOK) {
        numChildren = children.count;
        LOG_INFO("Number of children: %d", numChildren);
        // 输出每个子节点的名称
        for (int i = 0; i < numChildren; ++i) {
            LOG_INFO("Child node: %s", children.data[i]);
        }
    }else{
        char errtxt[512] = {0};
        sprintf(errtxt, "Failed to acquire sub service node from %s!", service_root_path.c_str());
        controller->SetFailed(errtxt);
        return;
    }
    int randi = rand() % numChildren;
    std::string method_path = service_root_path + "/" + children.data[randi] + "/" + method_name;
    std::string host_data = zkCli.GetData(method_path.c_str());
    if(host_data == ""){
        controller->SetFailed(method_path + " is not exist!");
        return;
    }
    int idx = host_data.find(":");
    if(idx == -1){
        controller->SetFailed(method_path + "address is invalid!");
        return;
    }
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx+1, host_data.size() - idx).c_str());
    LOG_INFO("the chosen node service path: %s, ip:%s, port:%d", method_path.c_str(), ip.c_str(), port);
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    // 连接rpc服务节点
    if(-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr))){
        char errtxt[512] = {0};
        sprintf(errtxt, "connect error! errno: %d", errno);
        controller->SetFailed(errtxt);
        close(clientfd);
        return;
    }

    // 发送rpc请求
    if(-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0)){
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno: %d", errno);
        controller->SetFailed(errtxt);
        close(clientfd);
        return;
    }

    // 接收rpc请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if(-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0))){
        char errtxt[512] = {0};
        sprintf(errtxt, "receive error! errno: %d", errno);
        controller->SetFailed(errtxt);
        close(clientfd);
        return;
    }
    // std::string response_str(recv_buf, 0, recv_size); // 这里有个bug, recv_buf中遇到\0后面的数据就存不下来了！！！
    // !!!!!!!!!!!!!!!!!!这里要好好总结，为什么会有/n？？？？学习学习调试
    if(!response->ParseFromArray(recv_buf, recv_size)){
        char errtxt[1050] = {0};
        sprintf(errtxt, "parse error! respnse_str: %s", recv_buf);
        controller->SetFailed(errtxt);
        close(clientfd);
        return;
    }

    close(clientfd);

    }
};