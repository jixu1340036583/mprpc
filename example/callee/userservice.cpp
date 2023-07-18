#include <iostream>
#include <string>
#include "user.pb.h"
#include "mprpcapplication.h"
#include "mprpcprovider.h"
#include "Logger.h"
using namespace std;
using namespace fixbug;


class UserService: public UserServiceRpc{
public:
    // // 假设UserService 当前是一个本地服务，提供了两个进程内的本地方法 Login和Register
    bool Login(std::string name, string pwd){
        cout << "doing local service:Login" << endl;
        cout << "name:" << name << " pwd:" << pwd << endl;
        return true;
    }
    bool Register(uint32_t id, std::string name, string pwd){
        cout << "doing local service:Register" << endl;
        cout << "id:" << id <<  " name:" << name << " pwd:" << pwd << endl;
        return true;
    }

    // 重写基类UserServiceRpc的虚函数 下面这些方法都是框架直接调用的
    // caller ===> Login(LoginRequest) ==> muduo ==> callee
    // callee ===> Login(LoginRequest) => 交到下面重写的这个Login方法
    void Login(::google::protobuf::RpcController* controller,
                       const ::fixbug::LoginRequest* request, // 函数参数
                       ::fixbug::LoginResponse* response, // 函数返回值
                       ::google::protobuf::Closure* done){ 
        // 框架给业务上报了请求参数LoginRequest，应用获取相应数据做本地业务
        string name = request->name();
        string pwd = request->pwd();
        // 做本地业务
        bool login_result = Login(name, pwd);
        // 把响应写入
        ResultCode* code = response->mutable_result();
        code->set_errcode(0);
        // code->set_errmsg("");
        response->set_success(login_result);
        
        // 执行回调操作, 执行rpc方法的返回值数据的序列化和网络发送（都是由框架来完成的）
        done->Run(); 
    }
    virtual void Register(::google::protobuf::RpcController* controller,
                       const ::fixbug::RegisterRequest* request,
                       ::fixbug::RegisterResponse* response,
                       ::google::protobuf::Closure* done){
        uint32_t id = request->id();
        string name = request->name();
        string pwd = request->pwd();
        bool ret = Register(id, name, pwd);
        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");
        response->set_success(ret);
        done->Run();
    }
};

int main(int argc, char** argv){
    // 调用框架的初始化操作
    rpc::MprpcApplication::Init(argc, argv);
    // 把UserService对象发布到rpc节点上
    rpc::MpRpcProvider provider;
    provider.NotifyService(new UserService());

    //启动一个rpc服务发布节点 Run以后，进程进入阻塞状态，等待远程的Rpc调用请求
    provider.Run();
    return 0;
}