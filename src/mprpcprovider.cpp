#include "mprpcprovider.h"
#include "rpcheader.pb.h"
#include "Logger.h"
#include "zookeeperutil.h"
namespace rpc{
// 这里是框架给外部使用的，可以发布rpc方法的函数接口
void MpRpcProvider::NotifyService(google::protobuf::Service *service){

    ServiceInfo service_info;
    // 获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor* pserviceDesc = service->GetDescriptor();
    // 获取服务的名字
    std::string service_name = pserviceDesc->name();
    LOG_INFO("Notify service_name: %s", service_name.c_str());
    // 获取服务对象service的方法的数量
    int methodCnt = pserviceDesc->method_count();
    for(int i = 0; i < methodCnt; ++i){
        const google::protobuf::MethodDescriptor* pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});
        LOG_INFO("Notify method_name: %s", method_name.c_str());
    }
    service_info.m_service = service; // 是为了获取调用函数的参数类型以及返回值类型
    m_serviceMap.insert({service_name, service_info});
}

// 启动rpc服务节点，开始提供rpc远程远程网络调用服务
void MpRpcProvider::Run(){
    // 组合了TcpServer
    std::unique_ptr<rx::TcpServer> m_tcpserverPtr;
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    rx::InetAddress address(port, ip);
    // 创建TcpServer对象
    rx::TcpServer server(&m_eventLoop, address, "MpRpcProvider");
    // 绑定连接回调和消息读写回调方法 分离了网络代码和业务代码
    server.setConnectionCallback(std::bind(&MpRpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&MpRpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    // 绑定muduo库的线程数量
    server.setLoopNum(4);

    // 把当前rpc节点上要发布的服务全部注册到zk上面，让rpc client
    // session timeout 30s zkclient 网络I/O线程 会在1/3 * timeout 时间发送ping(心跳消息)消息
    ZkClient zkCli;
    zkCli.Start();
    // service_name为永久性节点  method_name为临时性节点
    for(auto &sp: m_serviceMap){
        std::string service_path = "/" + sp.first + "_list";
        int exists = zoo_exists(zkCli.getZandle(), service_path.c_str() , 0, nullptr);
        if (exists == ZOK) 
            LOG_INFO("Root node %s exists!", service_path.c_str());
        else if (exists == ZNONODE) {
            LOG_INFO("Create Root node %s !", service_path.c_str());
            zkCli.Create(service_path.c_str(), nullptr, 0);
        } 
        else LOG_ERROR("Failed to check root node: %s !", service_path.c_str());
        std::string id = MprpcApplication::GetConfig().Load("rpcserverid");
        service_path = service_path + "/" + sp.first + id;
        zkCli.Create(service_path.c_str(), nullptr, 0);

        for(auto &mp: sp.second.m_methodMap){

            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            // ZOO_EPHEMERAL 表示node是个临时性节点
            zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }
    // 启动网络服务
    server.start();
    m_eventLoop.loop();
}

// 新的socket连接回调
void MpRpcProvider::OnConnection(const rx::TcpConnectionPtr& conn){
    if(!conn->connected()){
        // 和rpc client的连接断开了
        conn->shutdown();
    }
}


/*
在框架内部，MpRpcProvider和RpcConsumer协商好之间通信用的protobuf数据类型
service_name method_name args 定义相应的proto的message类型，进行数据的序列化与反序列化
*/ 

// 已建立连接用户的读写事件回调
void MpRpcProvider::OnMessage(const rx::TcpConnectionPtr& conn, rx::Buffer* buffer , Timestamp){
    std::string recv_buf = buffer->retrieveAllAsString();
    
    // 从字符流中读取前四个字节的内容
    uint32_t header_size = 0;
    recv_buf.copy((char*)&header_size, 4, 0);
    // 根据head_size读取数据头的原始字符流
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if(rpcHeader.ParseFromString(rpc_header_str)){
        // 数据反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }else{
        // 数据反序列化失败
        std::cout << "rpc_header_str: " << rpc_header_str << " parse error" << std::endl;
        return;
    }
    // 获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 打印调试信息
    std::cout << "===========================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "args_size: " << args_size << std::endl;
    std::cout << "===========================" << std::endl;

    
    // 获取service对象和method对象
    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end()){
        std::cout << service_name << " is not exist!" <<std::endl;
        return;
    }
    auto mit = it->second.m_methodMap.find(method_name);
    if(mit == it->second.m_methodMap.end()){
        std::cout << service_name << ":" << method_name << " is not exist!" <<std::endl;
    }  
    google::protobuf::Service* service = it->second.m_service; // 获取的service对象
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取method对象

    // 生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message* request = service->GetRequestPrototype(method).New();
    if(!request->ParsePartialFromString(args_str)){
        std::cout << "request parse error, content: " << args_str << std::endl;
    }
    google::protobuf::Message* response = service->GetResponsePrototype(method).New();
    
    // 给下面的method方法的调用，绑定一个Closure的回调函数
    google::protobuf::Closure* done = google::protobuf::NewCallback<MpRpcProvider, const rx::TcpConnectionPtr&, google::protobuf::Message*>(this, &MpRpcProvider::SendRpcResponse, conn, response);

    // 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
    service->CallMethod(method, nullptr, request, response, done);

}

void MpRpcProvider::SendRpcResponse(const rx::TcpConnectionPtr& conn, google::protobuf::Message* response){
    std::string response_str;
    if(response->SerializeToString(&response_str)){ // response进行序列化
        // 序列化成功后，通过网络把rpc方法执行的结果发送回给rpc的调用方
        conn->send(response_str);
    }
    else{
        std::cout << "serialize response_str error!" << std::endl;
    }
    conn->shutdown(); // 模拟http的短连接服务，由MpRpcProvider主动断开连接
}

};