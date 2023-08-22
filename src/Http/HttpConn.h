#ifndef HTTPCONN_H
#define HTTPCONN_H
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<sys/stat.h>
#include<string.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/mman.h>
#include<stdarg.h>
#include<errno.h>
#include<sys/wait.h>
#include<sys/uio.h>
#include<map>
#include<string>
#include"../SqlConnPool/SqlConnPool.h"
#include"../Locker/Locker.h"
using namespace std;
class HttpConn{
public:
    static const int FILENAME_LEN=200;
    static const int READ_BUFFER_SIZE=2048;
    static const int WRITE_BUFFER_SIZE=1024;
    enum METHOD{
        GET=0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE=0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE{
        NO_REQUEST,//请求不完整，需要继续读取请求报文数据
        GET_REQUEST,//获得了完整的http请求
        BAD_REQUEST,//请求报文存在语法错误
        NO_RESOURCE,//请求资源不存在
        FORBIDDEN_REQUEST,//请求被服务器拒绝
        FILE_REQUEST,//请求资源可以正常访问，跳转precess_write完成响应报文
        INTERNAL_ERROR,//服务器内部错误
        CLOSED_CONNECTION
    };
    enum LINE_STATUS{
        LINE_OK=0,
        LINE_BAD,
        LINE_OPEN
    };
public:
    HttpConn(){}
    ~HttpConn(){}
    void init(int sockfd,const sockaddr_in &addr,char *,int,int,string user,string passwd,string sqlname);
    //关闭http连接
    void close_conn(bool real_close = true);
    void process();
    //读取游览器端发来的全部数据
    bool read_once();
    //将数据发送给请求端
    bool write();
    sockaddr_in *get_address(){return &m_address;}

    void init_mysql_map(SqlConnPool *sqlconnpool);
    int timer_flag;
    int improv;
private:
    void init();
    HTTP_CODE process_read();
    char *get_line(){return m_read_buf+m_last_line_idx;}
    //从状态机读取一行
    LINE_STATUS parse_line();
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_header(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    bool process_write(HTTP_CODE);

    bool add_response(const char *format, ...);
    bool add_status_line(int status,const char *title);
    bool add_header(int content_length);
    bool add_content(const char *content);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
    bool add_content_type();

    void unmap();
    

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL* mysql;
    int m_state;
private:
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];
    long m_read_idx;
    long m_checked_idx;
    int m_last_line_idx;

    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;

    //主状态机大状态
    CHECK_STATE m_check_state;
    //请求方法
    METHOD m_method;


    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    long m_content_length;
    bool m_linger;//区分是keep-alive还是close

    //服务器上的文件地址
    char *m_file_address;
    struct stat m_file_stat;
    //io向量机制iovec
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;
    char *m_string;//存储请求头数据

    int bytes_to_send;//剩余发送字节数
    int bytes_have_send;//已发送字节数
    char *doc_root;

    map<string,string> m_users; //存储数据库中数据的map
    int m_TRIGMode;
    int m_close_log; //是否开启日志

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];

};
#endif
