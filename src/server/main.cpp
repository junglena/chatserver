
#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h> // 用于 signal 函数

// 引入 muduo 相关的命名空间
using namespace muduo;
using namespace muduo::net;

// 处理服务器 ctrl+c 结束后，重置 user 的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc,char **argv)
{
    if(argc<3)
    {
        cerr<<"command invalid"<<endl;
    }
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);
    // 注册信号捕捉，捕捉 SIGINT (Ctrl+C)
    signal(SIGINT, resetHandler);

    // 创建事件循环对象
    EventLoop loop;

    // 设置服务器的 IP 和 Port
    InetAddress addr(ip, port);

    // 创建服务器对象
    ChatServer server(&loop, addr, "ChatServer");

    // 启动网络服务
    server.start();
    std::cout << "ChatServer is running" << std::endl;

    // 开启事件循环，这是一个阻塞操作，服务器会一直运行在此
    loop.loop();

    return 0;
}