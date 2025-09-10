//网络层，核心任务是处理底层的网络连接和数据收发，然后将解析好的请求“分发”给“业务层”(ChatService) 去处理
#include "chatserver.hpp"
#include<functional>
#include<string>
#include"nlohmann/json.hpp"
#include"chatservice.hpp"
using json = nlohmann::json;
using namespace std;
using namespace placeholders;
ChatServer::ChatServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const string &nameArg)
               :_server(loop,listenAddr,nameArg),_loop(loop)
{
    //注册连接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));
    //注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));
    //设置线程数量
    _server.setThreadNum(4);
}
void ChatServer::start()
{
    _server.start();
}
//上传链接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    //客户端断开链接
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}
//上传读写事件相关的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                   Buffer *buffer,
                   Timestamp receiveTime)
{
    string buf = buffer->retrieveAllAsString();
    //数据的反序列化
    json js = json::parse(buf);
    //达到的目的：完全解耦网络模块的代码和业务模块的代码
    //通过js["msgid"]获取-->业务handler-->conn  js  receivetime
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>()); 
    //网络层再次调用业务层，说：“我收到了一个类型为 msgid 的消息，请把处理这种消息的专用函数（Handler）给我。”
    
    //回调消息绑定好的事件处理器来解决事物
    msgHandler(conn,js,receiveTime);
}