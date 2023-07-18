#include "mprpcapplication.h"
#include <iostream>
#include <unistd.h>
namespace rpc{
MprpcConfig MprpcApplication::m_config; // 类外定义静态成员变量

void showArgsHelp(){
    std::cout<< "format: command -i <configfile>" << std::endl;
}

void MprpcApplication::Init(int argc, char** argv){
    if(argc < 2){
        showArgsHelp();
        exit(EXIT_FAILURE); // 异常退出程序
    }
    int c = 0;
    std::string config_file;
    // https://blog.csdn.net/afei__/article/details/81261879
    while((c = getopt(argc, argv, "i:")) != -1){  // 带冒号表示后面必须有参数
        switch(c){
            case 'i':
                config_file = optarg;
                break;
            case '?':
                std::cout << "invalid args!" << std::endl;
                showArgsHelp();
                exit(EXIT_FAILURE);
            default:
                break;
        }
    }
    // 开始加载配置文件 rpcserver_ip =  rpcserver_port zookeeper_ip zookeeper_port
    m_config.LoadConfigFile(config_file.c_str());

}

MprpcApplication& MprpcApplication::GetInstance(){
    static MprpcApplication app;
    return app;
}

MprpcApplication::MprpcApplication(){

}


MprpcConfig& MprpcApplication::GetConfig(){
    return m_config;
}
};