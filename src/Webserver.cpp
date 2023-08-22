#include"Webserver.h"

WebServer::WebServer(){
    users=new HttpConn[MAX_FD];

    char server_path[200];
    getcwd(server_path,200);
    char root[6]="/root";
    m_root=(char *)malloc(strlen(server_path)+strlen(root)+1);

    strcpy(m_root,server_path);
    strcat(m_root,root);

    //timer
    client_data=new ClientData[MAX_FD];
}

WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete[] users;
    delete[] client_data;
    delete m_threadPool;
}

void WebServer::init(int port,string user,string password,string databasename,
    int log_write,int opt_linger,int trigmode,int sql_num,int thread_num,
    int close_log,int actor_model){
        m_port=port;
        m_user=user;
        m_passwd=password;
        m_databasename=databasename;
        m_log_write=log_write;
        m_opt_linger=opt_linger;
        m_TRIGMode=trigmode;
        m_sqlConn_num=sql_num;
        m_thread_num=thread_num;
        m_close_log=close_log;
        m_actormodel=actor_model;
}

void WebServer::trig_mode(){
    switch(m_TRIGMode){
        case 0:
        {
            m_LISTENTrigmode=0;
            m_CONNTrigmode=0;
            break;
        }
        case 1:
        {
            m_LISTENTrigmode=0;
            m_CONNTrigmode=1;
            break;
        }
        case 2:
        {
            m_LISTENTrigmode=1;
            m_CONNTrigmode=0;
            break;
        }
        case 3:
        {
            m_LISTENTrigmode=1;
            m_CONNTrigmode=1;
            break;
        }
        default:
            break;
    }
}

void WebServer::log_write(){
    if(0==m_close_log){
        if(1==m_log_write)
            Log::getInstance()->init("./ServerLog",m_close_log,200,800000,800);
        else
            Log::getInstance()->init("./ServerLog",m_close_log,2000,800000,0);
    }
}

void WebServer::sql_pool(){
    m_sqlConnPool=SqlConnPool::getInstance();

    m_sqlConnPool->init("localhost",m_user,m_passwd,m_databasename,3306,m_sqlConn_num,m_close_log);

    users->init_mysql_map(m_sqlConnPool);
}

void WebServer::thread_pool(){
    m_threadPool=new ThreadPool<HttpConn>(m_actormodel,m_sqlConnPool,m_thread_num);
}

void WebServer::event_listen(){
    extern FILE *mainfp;
    m_listenfd=socket(PF_INET,SOCK_STREAM,0);
    //LOG_INFO("%s,[%d]","listenfd create!",m_listenfd);
    fputs("listenfd create!\n",mainfp);
    fflush(mainfp);
    assert(m_listenfd>=0);

    //优雅关闭连接
    if(0==m_opt_linger){
        struct linger tmp={0,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }
    else if(1==m_opt_linger){
        struct linger tmp={1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }

    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(m_port);

    //设置套接字地址重用功能
    int flag=1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));

    int ret=0;
    ret=bind(m_listenfd,(struct sockaddr *)&address,sizeof(address));
    fputs("listenfd bind!\n",mainfp);
    fflush(mainfp);
    assert(ret>=0);
    ret=listen(m_listenfd,5);
    fputs("listenfd listen!\n",mainfp);
    fflush(mainfp);
    assert(ret>=0);

    utils.init(TIMESLOT);

    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd=epoll_create(5);
    assert(m_epollfd!=-1);

    utils.addfd(m_epollfd,m_listenfd,false,m_TRIGMode);
    HttpConn::m_epollfd=m_epollfd;

    ret=socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd);
    assert(ret!=-1);
    utils.setnonblocking(m_pipefd[1]);//写端非阻塞
    utils.addfd(m_epollfd,m_pipefd[0],false,0);//读端ET非阻塞

    utils.add_sig(SIGPIPE,SIG_IGN);
    utils.add_sig(SIGALRM,utils.sig_handler,false);
    utils.add_sig(SIGTERM,utils.sig_handler,false);

    alarm(TIMESLOT);

    Utils::u_pipefd=m_pipefd;
    Utils::u_epollfd=m_epollfd;
}

void WebServer::timer(int connfd,struct sockaddr_in client_address)
{
    //HttpConn数组
    users[connfd].init(connfd,client_address,m_root,m_CONNTrigmode,m_close_log,
    m_user,m_passwd,m_databasename);//here

    client_data[connfd].addr=client_address;
    client_data[connfd].sockfd=connfd;

    ClassTimer *timer=new ClassTimer;
    timer->client_data=&client_data[connfd];
    timer->cb_func=cb_func;
    time_t cur=time(NULL);
    timer->expire=cur+3*TIMESLOT;

    client_data[connfd].timer=timer;

    utils.m_timer_lst.add_timer(timer);
    LOG_INFO("add timer SUCCESS;timer: %s",timer);

}

void WebServer::adjust_timer(ClassTimer *timer){
    time_t cur=time(NULL);
    timer->expire=cur+3*TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s","adjust timer once");
}

void WebServer::deal_timer(ClassTimer *timer,int sockfd){
    timer->cb_func(&client_data[sockfd]);
    if(timer){
        utils.m_timer_lst.del_timer(timer);
    }
    LOG_INFO("close fd %d",client_data[sockfd].sockfd);
}

bool WebServer::deal_clientdata(){
    struct sockaddr_in client_address;
    socklen_t client_addrlength=sizeof(client_address);
    if(0==m_LISTENTrigmode){
        int connfd=accept(m_listenfd,(struct sockaddr *)&client_address,&client_addrlength);
        if(connfd<0){
            LOG_ERROR("%s:errno is:%d","accept error",errno);
            return false;
        }
        if(HttpConn::m_user_count>=MAX_FD){
            utils.show_error(connfd,"Interanal server busy");
            LOG_ERROR("%s","Interanal server busy");
            return false;
        }
        LOG_INFO("accept SUCCESS;connfd:%d",connfd);
        timer(connfd,client_address);
    }
    else{
        while(1){
            int connfd=accept(m_listenfd,(struct sockaddr *)&client_address,&client_addrlength);
            if(connfd<0){
                LOG_ERROR("%s:errno is:%d","accept error",errno);
                return false;
            }
            if(HttpConn::m_user_count>=MAX_FD){
                utils.show_error(connfd,"Interanal server busy");
                LOG_ERROR("%s","Interanal server busy");
                return false;
            }
            timer(connfd,client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::deal_signal(bool &timeout,bool &stop_server){
    int ret=0;
    int sig;
    char signals[1024];
    ret=recv(m_pipefd[0],signals,sizeof(signals),0);
    if(ret==-1){
        return false;
    }
    else if(ret==0){
        return false;
    }
    else{
        for(int i=0;i<ret;i++){
            switch(signals[i]){
                case SIGALRM:
                {
                    timeout=true;
                    break;
                }
                case SIGTERM:
                {
                    stop_server=true;
                    break;
                }
            }
        }
    }
    return true;
}

void WebServer::deal_read(int sockfd){
	ClassTimer *timer=client_data[sockfd].timer;

	//reactor
	if(1==m_actormodel){
        if(timer){
            adjust_timer(timer);
        }
        m_threadPool->append(users+sockfd,0);

        while(true){
            if(1==users[sockfd].improv){
                if(1==users[sockfd].timer_flag){
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag=0;
                }
                users[sockfd].improv=0;
                break;
            }
        }
    }
    //proactor
    else{
        if(users[sockfd].read_once()){
            LOG_INFO("deal with the client(%s)",inet_ntoa(users[sockfd].get_address()->sin_addr));

            m_threadPool->append_p(users+sockfd);

            if(timer){
                adjust_timer(timer);
            }
        }
        else{
            deal_timer(timer,sockfd);
        }
    }
}

void WebServer::deal_write(int sockfd){
    ClassTimer *timer=client_data[sockfd].timer;
    //reactor
    if(1==m_actormodel){
        if(timer){
            adjust_timer(timer);
        }
        m_threadPool->append(users+sockfd,1);

        while(true){
            if(1==users[sockfd].improv){
                if(1==users[sockfd].timer_flag){
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag=0;
                }
                users[sockfd].improv=0;
                break;
            }
        }
    }
    //proactor
    else{
        if(users[sockfd].write()){
            LOG_INFO("send data to the client(%s)",inet_ntoa(users[sockfd].get_address()->sin_addr));

            if(timer){
                adjust_timer(timer);
            }
        }
        else{
            deal_timer(timer,sockfd);
        }
    }
}

void WebServer::event_loop(){
    extern FILE *mainfp;
    bool timeout=false;
    bool stop_server=false;

    while(!stop_server){
        int number=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        fputs("epoll_wait after\n",mainfp);
        fflush(mainfp);
        if(number<0&&errno!=EINTR){
            LOG_ERROR("%s","epoll failure");
            break;
        }

        for(int i=0;i<number;i++){
            int sockfd=events[i].data.fd;

            if(sockfd==m_listenfd){
                fputs("have connect\n",mainfp);
                fflush(mainfp);
                bool flag=deal_clientdata();
                if(false==flag)
                    continue;
            }
            else if(events[i].events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR)){
                ClassTimer *timer=client_data[sockfd].timer;
                deal_timer(timer,sockfd);
            }
            else if((sockfd==m_pipefd[0])&&(events[i].events&EPOLLIN)){
                bool flag=deal_signal(timeout,stop_server);
                if(flag==false){
                    LOG_ERROR("%s","dealclinetdata failure");
                }
            }
            else if(events[i].events&EPOLLIN){
                deal_read(sockfd);
            }
            else if(events[i].events&EPOLLOUT){
                deal_write(sockfd);
            }
        }

        if(timeout){
            utils.timer_handler();

            LOG_INFO("%s","timer tick");

            timeout=false;
        }
    }
}