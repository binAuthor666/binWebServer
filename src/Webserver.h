#ifndef WEBSERVER_H
#define WEBSERVER_H

#include<sys/epoll.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<stdlib.h>
#include<assert.h>
#include"./Timer/LstTimer.h"
#include"./Http/HttpConn.h"
#include"./ThreadPool/ThreadPool.h"


const int MAX_FD=65536;
const int MAX_EVENT_NUMBER=10000;
const int TIMESLOT=5;

class WebServer{
public:
    WebServer();
    ~WebServer();

    void init(int port,string user,string password,string databasename,
    int log_write,int opt_linger,int trigmode,int sql_num,int thread_num,
    int close_log,int actor_model);

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();

    void event_listen();
    void event_loop();

    void timer(int connfd,struct sockaddr_in client_address);
    void adjust_timer(ClassTimer *timer);
    void deal_timer(ClassTimer *timer,int sockfd);
    bool deal_clientdata();
    bool deal_signal(bool &timeout,bool &stop_server);
    void deal_read(int sockfd);
    void deal_write(int sockfd);

public:
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    HttpConn *users;

    SqlConnPool *m_sqlConnPool;
    string m_user;
    string m_passwd;
    string m_databasename;
    int m_sqlConn_num;

    ThreadPool<HttpConn> *m_threadPool;
    int m_thread_num;

    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_opt_linger;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    ClientData *client_data;
    Utils utils;


};

#endif