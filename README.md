# mprpc

## 内容列表

- [背景](#背景)
- [前置模块讲解](#前置模块讲解)
- [mprpc架构设计](#架构设计)
- [系统的不足之处](#系统的不足之处)
- [项目中遇到的问题](#项目中遇到的问题)
- [数据库表结构](#数据库表结构)
- [安装说明](#安装说明)


## 背景
+ 本项目是一个自定义的 RPC（远程过程调用）框架，结合使用 Protocol Buffers（Protobuf）和 ZooKeeper;
+ 它旨在提供一种简单、可靠且高度可扩展的方式来实现分布式系统中的跨网络方法调用;
+ 此外，为了可将服务节点集群部署并且实现负载均衡，我采用随机选择的策略每次返回给客户端不同的服务节点;

## 前置模块讲解
网络库：[reactorX](https://github.com/jixu1340036583/reactorX)

线程池：[ThreadPool](https://github.com/jixu1340036583/ThreadPool)


## 架构设计
### 服务发布模块MpRpcProvider
MpRpcProvider做的事情主要是服务注册到zookeeper，开启事件循环与rpc调用方通信。

**服务发布模块MpRpcProvider::NotifyService()**

+ 首先，该方法会接受一个要注册到zookeeper中的服务对象。
+ 然后获取到当前发布的方法名以及方法对应的描述符，将它们作为一对pair存储到map中，进而再与服务对象的指针组成一个service_info结构体。然后服务名称和service_info再组成一个map存到MpRpcProvider对象中，以供MprpcChannel查找调用。

**MpRpcProvider::Run()**

+ 在Run()使用我自己实现的ReactorX网络库启动事件循环，接收调用方请求。具体说来：
+ 首先定义m_eventLoop对象和TcpServer对象；
+ 连接zk服务端；
+ 为了将服务节点部署为集群，在发布每个服务节点前要首先判断每个服务节点的根节点是否存在(for example：/UserServiceRpc_list/)，不在的话则先创建根节点；
+ 然后创建当前服务节点的znode节点，由于是集群部署，因此要为每个服务节点名后面加一个编号以用来区分；
+ 进而创建叶子节点以及对应的ip:port;
+ 最终启动主反应堆，开启事件循环;

**对于读事件回调OnMessage()来说**
+ 接收到数据包后，首先解析出前四个字节获取rpc_header的长度；进而就可以得到服务名，方法名，参数长度。进而解析出参数；
+ 根据服务名和方法名就可以得到服务对象以及方法描述符；根据方法描述符可以得到该方法的requst类型和response类型；
+ 然后就可以根据CallMethod来调用相应的方法了，注意这里要绑定一个回调，将response序列化发回给给rpc调用方。


### rpc通道MprpcChannel模块
顾名思义，Channel为通道之义，它是rpc调用方和rpc服务方之间的通道。MprpcChannel继承于google::protobuf::RpcChannel类，实现MprpcChannel只需要重写其中的CallMethod()，因为rpc调用方使用stub调用方法时都要通过这个CallMethod()。具体实现为：
+ 首先要获得调用方法的服务名和方法名。
+ 然后将rpcHeader的大小，rpcHeader(service_name、method_name、args_size)，args_str，打包成一个数据包;
+ 连接zk服务端，首先判断当前函数所在的服务根节点是否存在；
+ 在的话获取所有活跃的服务节点，采用随机选择的策略选中一个节点作为真正的服务节点；
+ 最后一步就是将数据包发给服务端，等待服务端响应。然后解析response，返回给用户。


### mprpccontroller模块
mprpccontroller模块其实很简单，它相当于一个日志模块，可以记录在在rpc调用过程中最后一次发生的错误返回给调用方。



## 系统的不足之处
目前的rpc调用只支持同步阻塞调用，还不支持异步调用。



## 项目中遇到的问题



## 安装说明

开发环境：Ubuntu VsCode

编译器：g++

编译工具：CMake

编程语言：C++

protobuf: protobuf-cpp-3.21.9

Zookeeper: zookeeper-3.4.10
