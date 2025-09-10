#pragma once
#include<unordered_map>
#include<functional>
#include<muduo/net/TcpConnection.h>
#include<mutex>
#include"nlohmann/json.hpp"
#include"usermodel.hpp"
#include"friendmodel.hpp"
#include"groupmodel.hpp"
#include"redis.hpp"
#include"offlinemessagemodel.hpp"
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;
using MsgHandler = std::function<void(const TcpConnectionPtr &conn,json &js,Timestamp)>;

//聊天服务器业务类
class ChatService
{
private:
    ChatService();
    //存储消息id和其对应的业务的处理方法
    unordered_map<int,MsgHandler>_msgHandlerMap;
    //存储在线用户的通信连接
    unordered_map<int,TcpConnectionPtr> _userConnMap;
    //定义互斥锁，保证_userConnMap线程安全
    mutex _connMutex;
    //数据操作类对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendmodel;
    GroupModel _groupmodel;

    //redis操作对象
    Redis _redis;
public:
    //获取单例对象的接口函数
    static ChatService* instance();
    //处理登陆业务
    void login(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //一对一聊天业务
    void onechat(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //添加好友业务
    void addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //创建群组
    void createGroup(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //加入群组
    void addGroup(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //查询用户所在的群组信息
    void groupchat(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //注销
    void loginout(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
    //服务器异常，业务重置方法
    void reset();

    void handleRedisSubscribeMessage(int,string);
};