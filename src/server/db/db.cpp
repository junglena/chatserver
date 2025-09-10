#include"db.h"
#include<mysql/mysql.h>
#include<string>
#include<muduo/base/Logging.h>
using namespace std;

static string server = "127.0.0.1";
static string user = "root";
static string password = "jzjjzj200203";
static string dbname = "chat";



//初始化数据库连接
MySQL::MySQL()
{
    _conn = mysql_init(nullptr);
}
//释放数据库连接资源
MySQL::~MySQL()
{
    if(_conn!=nullptr)
    {
        mysql_close(_conn);
    }
}
//连接数据库
bool MySQL::connect()
{
    MYSQL* p = mysql_real_connect(_conn, server.c_str(), user.c_str(), 
    password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    return p != nullptr;
}
//更新操作
bool MySQL::update(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO<<__FILE__<<":"<<__LINE__<<":"<<sql<<"更新失败！";
        return false;
    }
    return true;
}
MYSQL_RES* MySQL::query(string sql)
{
    //执行查询语句
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO<<__FILE__<<":"<<__LINE__<<":"<<sql<<"查询失败！";
        return nullptr;
    }
    return mysql_store_result(_conn);
}
MYSQL* MySQL::getConnection()
{
    return _conn;
}