#include"chatservice.hpp"
#include"public.hpp"
#include"friendmodel.hpp"
#include"redis.hpp"
#include"muduo/base/Logging.h"
#include<vector>
#include<map>
#include<string>
using namespace std;
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}
//注册消息以及对应的handler回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::onechat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
     // 注册“创建群组”消息的处理器
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    // 注册“加入群组”消息的处理器
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    // 注册“群组聊天”消息的处理器
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupchat, this, _1, _2, _3)});
    //处理注销业务
    _msgHandlerMap.insert({LOGIN_OUT_MSG,std::bind(&ChatService::loginout,this,_1,_2,_3)});
    //连接redis
    if(_redis.connect())
    {
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}
//服务器异常，业务重置方法
void ChatService::reset()
{
    //online->offline
    _userModel.resetstate();
}
//处理登陆业务
void ChatService::login(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    if(user.getId()==id && user.getPwd()==pwd)
    {
        if(user.getState()=="online")
        {
            //该用户已登陆，不允许重复登陆
            json response;// 创建一个JSON对象用于构建响应消息
            response["msgid"] = LOGIN_MSG_ACK;// 设置消息类型
            response["errno"] = 2;// 设置错误码为1，表示错误
            response["errmsg"] = "该账号已经登陆，请重新输入新账号！";
            conn->send(response.dump());

        }
        else
        {
            //记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id,conn});
            }
            //id用户登陆成功后，向redis订阅channel(id)
            _redis.subscribe(id);
            //登陆成功 更新用户状态信息offline->online
            user.setState("online");
            _userModel.update(user);

            json response;// 创建一个JSON对象用于构建响应消息
            response["msgid"] = LOGIN_MSG_ACK;// 设置消息类型
            response["errno"] = 0;// 设置错误码为0，表示没有错误
            response["id"] = user.getId();
            response["name"] = user.getName();
            //查询该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"] = vec;
                //读取后，把该用户的所有离线消息删除
                _offlineMsgModel.remove(id);
            }
            //查询该好友的好友信息并返回
            vector<User> uservec = _friendmodel.query(id);
            if(!uservec.empty())
            {
                vector<string> vec2;
                for(User &user : uservec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"]= user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            //查询用户的群组信息
            vector<Group> groupuserVec = _groupmodel.queryGroups(id);
            if(!groupuserVec.empty())
            {
                vector<string> GroupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for(GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    GroupV.push_back(grpjson.dump());
                }
                response["groups"] = GroupV;
            }
            conn->send(response.dump());
        }
        
    }
    else
    {
        //该用户不存在,登陆失败
        json response;// 创建一个JSON对象用于构建响应消息
        response["msgid"] = LOGIN_MSG_ACK;// 设置消息类型
        response["errno"] = 1;// 设置错误码为1，表示错误
        response["errmsg"] = "用户名或密码错误！";
        conn->send(response.dump());
    }
}
//处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];
    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if(state)
    {
        //注册成功
        json response;// 创建一个JSON对象用于构建响应消息
        response["msgid"] = REG_MSG_ACK;// 设置消息类型为注册响应(REG_MSG_ACK)，告诉客户端这是对注册请求的响应
        response["errno"] = 0;// 设置错误码为0，表示没有错误，注册成功
        response["id"] = user.getId();// 将新注册用户的ID放入响应中，这样客户端就知道自己注册成功并获得了一个用户ID
        conn->send(response.dump());// 将JSON对象序列化为字符串并通过TCP连接发送给客户端
                                    // dump()方法将JSON对象转换为字符串格式
    }
    else
    {
        //注册失败
        json response;// 创建一个JSON对象用于构建响应消息
        response["msgid"] = REG_MSG_ACK;// 设置消息类型为注册响应(REG_MSG_ACK)，告诉客户端这是对注册请求的响应
        response["errno"] = 1;// 设置错误码为1
        conn->send(response.dump());// 将JSON对象序列化为字符串并通过TCP连接发送给客户端
                                    // dump()方法将JSON对象转换为字符串格式
    }
}
//处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it!=_userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }
    //下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);
    //更新用户的状态信息
    User user(userid,"","","offline");
    _userModel.update(user);
}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    //记录错误日记
    auto it = _msgHandlerMap.find(msgid);
    if(it==_msgHandlerMap.end())
    {
        return [=](const TcpConnectionPtr &conn,json &js,Timestamp time){
            LOG_ERROR << "msgid:"<< msgid << "cna not find handler!";
        }; 
    }
    else
    {
        return _msgHandlerMap[msgid];
    } 
}
//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it = _userConnMap.begin();it!=_userConnMap.end();it++)
        {
            if(it->second==conn)
            {
                user.setId(it->first);
                //从map表中删除用户的连接信息
                _userConnMap.erase(it);
                break;
            }
        }
    }
    
    _redis.unsubscribe(user.getId());
    if(user.getId()!=-1)
    {
        //更新用户的状态信息
        user.setState("offline");
        _userModel.update(user);
    }
}
//一对一聊天业务
void ChatService::onechat(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int toid = js["to"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it !=_userConnMap.end())
        {
            //toid在线，转发消息  服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
        
        //查询toid是否在线
        User user = _userModel.query(toid);
        if(user.getState()=="online")
        {
            _redis.publish(toid,js.dump());
            return;
        }

        //toid不在线,存储离线消息
        _offlineMsgModel.insert(toid,js.dump());
    }
}
//添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    //存储好友信息
    _friendmodel.insert(userid,friendid);
}

//创建群聊业务
void ChatService::createGroup(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    //存储新创建的群组信息
    Group group(-1,name,desc);
    if(_groupmodel.createGroup(group))
    {
        //存储群组创建人信息
        _groupmodel.addGroup(userid,group.getId(),"creator");
    }
}
//加入群组
void ChatService::addGroup(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupmodel.addGroup(userid,groupid,"normal");
}
//群组聊天业务
void ChatService::groupchat(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupmodel.queryGroupUsers(userid,groupid);
    lock_guard<mutex> lock(_connMutex);
    for(int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if(it!=_userConnMap.end())
        {
            it->second->send(js.dump());
        }
        else
        {
            User user = _userModel.query(id);
            if(user.getState()=="online")
            {
                _redis.publish(id,js.dump());
            }
            else
            {
                _offlineMsgModel.insert(id,js.dump());
            }
        }    
    }
}
void ChatService::handleRedisSubscribeMessage(int userid,string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it!=_userConnMap.end())
    {
        it->second->send(msg);
        return;
    }
    _offlineMsgModel.insert(userid,msg);
}
