#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H
#include<string>
#include<list>
#include<mysql/mysql.h>
#include"../Sem/Sem.h"
#include"../Locker/Locker.h"
#include"../Log/Log.h"
class SqlConnPool {
public:
	static SqlConnPool *getInstance();
	MYSQL *getConnection();
	bool releaseConnection(MYSQL* conn);
	void destroyPool();
	int getFreeConn();

	void init(std::string url, std::string user, std::string password, std::string databaseName,int port, int max_conn, int close_log);
private:
	SqlConnPool();
	~SqlConnPool();
	int m_max_conn;
	int m_free_conn;
	int m_used_conn;
	std::list<MYSQL*> connList;
	Sem connPoolSem;
	Locker connPoolLocker;

public:
	std::string m_url;	//主机地址
	int m_port;	//数据库端口号
	std::string m_user;
	std::string m_password;
	std::string m_databaseName;
	int m_close_log;	//日志开关
};

class SqlConnectionRAII {
public:
	SqlConnectionRAII(MYSQL ** conn,SqlConnPool* pool);
	~SqlConnectionRAII();
private:
	MYSQL* connRAII;
	SqlConnPool* poolRAII;
};
#endif // !SQLCONNPOOL_H
