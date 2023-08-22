#include"SqlConnPool.h"

SqlConnPool::SqlConnPool() {
	m_free_conn=0;
	m_used_conn=0;
}

SqlConnPool::~SqlConnPool() {
	destroyPool();
}

SqlConnPool* SqlConnPool::getInstance() {
	static SqlConnPool instance;
	return &instance;
}

void SqlConnPool::init(std::string url,std::string user, std::string password, std::string databaseName, int port, int max_conn, int close_log){

    m_url=url;
    m_port=port;
    m_user=user;
    m_password=password;
    m_databaseName=databaseName;
    m_close_log=close_log; 


	for (int i = 0;i < max_conn;i++)
	{
		MYSQL* conn = NULL;
		conn = mysql_init(conn);
		if (conn == NULL) {
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		conn = mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(), databaseName.c_str(), port, NULL,0);
		if (conn == NULL) {
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		connList.push_back(conn);
		++m_free_conn;
	}
	connPoolSem = Sem(m_free_conn);
	m_max_conn=m_free_conn;
}

int SqlConnPool::getFreeConn() { 
	return this->m_free_conn;
}

void SqlConnPool::destroyPool() {
	connPoolLocker.lock();
	if (connList.size() > 0) {
		std::list<MYSQL*>::iterator it;
		for (it = connList.begin();it != connList.end();++it) {
			MYSQL* cur = *it;
			mysql_close(cur);
		}
		m_free_conn = 0;
		m_used_conn = 0;
		connList.clear();

	}
	connPoolLocker.unlock();
}

MYSQL* SqlConnPool::getConnection() {

	MYSQL* cur=NULL;
	if (connList.size() == 0) {
		return NULL;
	}
	connPoolSem.wait();
	connPoolLocker.lock();

	cur=connList.front();
	connList.pop_front();
	--m_free_conn;
	++m_used_conn;
	connPoolLocker.unlock();
	return cur;
}
bool SqlConnPool::releaseConnection(MYSQL* conn) {
	if (conn==NULL) {
		return false;
	}
	connPoolLocker.lock();
	connList.push_back(conn);
	++m_free_conn;
	--m_used_conn;
	connPoolLocker.unlock();
	connPoolSem.post();

	return true;
}

SqlConnectionRAII::SqlConnectionRAII(MYSQL** conn, SqlConnPool* pool) {
	*conn=pool->getConnection();
	connRAII = *conn;
	poolRAII = pool;
}








SqlConnectionRAII::~SqlConnectionRAII() {
	poolRAII->releaseConnection(connRAII);
}
