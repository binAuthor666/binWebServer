#include"HttpConn.h"
#include<fstream>

const char *ok_200_title="OK";
const char *error_400_title="Bad Request";
const char *error_400_form="Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title="Forbidden";
const char *error_403_form="You do not have permission to get file form this server.\n";
const char *error_404_title="Not Found";
const char *error_404_form="The requested file was not found on this server.\n";
const char *error_500_title="Internal Error";
const char *error_500_form="There was an unusual problem serving the request file.\n";

Locker m_locker;
map<string,string> users;
int HttpConn::m_epollfd=-1;
int HttpConn::m_user_count=0;

int setnonblocking(int fd){
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void removefd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

void addfd(int epollfd,int fd,bool one_shot,int TRIGMode){
    epoll_event event;
    event.data.fd=fd;

    if(1==TRIGMode)
        event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
    else
        event.events=EPOLLIN|EPOLLRDHUP;
    if(one_shot)
        event.events|=EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void modfd(int epollfd,int fd,int ev,int TRIGMode){
    epoll_event event;
    event.data.fd=fd;

    if(1==TRIGMode){
        event.events=ev|EPOLLET|EPOLLONESHOT|EPOLLRDHUP;
    }
    else{
        event.events=ev|EPOLLONESHOT|EPOLLRDHUP;
    }
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}


void HttpConn::init(int sockfd,const sockaddr_in &addr,char *root,int TRIGMode,int close_log,
string user,string passwd,string sqlname){
    m_sockfd=sockfd;
    m_address=addr;

    addfd(m_epollfd,sockfd,true,m_TRIGMode);
    m_user_count++;

    //当游览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root=root;
    m_TRIGMode=TRIGMode;
    m_close_log=close_log;

    strcpy(sql_user,user.c_str());
    strcpy(sql_passwd,passwd.c_str());
    strcpy(sql_name,sqlname.c_str());

    init();
}
void HttpConn::init(){
    mysql=NULL;
    bytes_to_send=0;
    bytes_have_send=0;
    m_check_state=CHECK_STATE_REQUESTLINE;
    m_linger=false;
    m_method=GET;
    m_url=0;
    m_version=0;
    m_content_length=0;
    m_host=0;
    m_last_line_idx=0;
    m_checked_idx=0;
    m_read_idx=0;
    m_write_idx=0;
    cgi=0;
    m_state=0;
    timer_flag=0;
    improv=0;

    memset(m_read_buf,'\0',READ_BUFFER_SIZE);
    memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
    memset(m_real_file,'\0',FILENAME_LEN);
}

HttpConn::HTTP_CODE HttpConn::process_read(){
    //LOG_INFO("INTO process_read\n");
    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char *text=0;

    while((m_check_state==CHECK_STATE_CONTENT&&line_status==LINE_OK)||(line_status=parse_line())==LINE_OK){
        text=get_line();
        m_last_line_idx=m_checked_idx;
        LOG_INFO("%s",text);
        switch(m_check_state){
            case CHECK_STATE_REQUESTLINE:
            {
                ret=parse_request_line(text);
                if(ret==BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret=parse_header(text);
                if(ret==BAD_REQUEST)
                    return BAD_REQUEST;
                //完整解析get请求后，跳转到报文响应函数
                else if(ret==GET_REQUEST)
                    return do_request();
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret=parse_content(text);
                if(ret==GET_REQUEST)
                    return do_request();
                //解析完消息体，设为LINE_OPEN,为了跳出循环
                line_status=LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HttpConn::LINE_STATUS HttpConn::parse_line(){
    //LOG_INFO("into parse_line(从状态机分析)\n");
    char temp;
    for(;m_checked_idx<m_read_idx;++m_checked_idx){
        temp=m_read_buf[m_checked_idx];
        if(temp=='\r'){
            if((m_checked_idx+1)==m_read_idx)
                return LINE_OPEN;
            else if(m_read_buf[m_checked_idx+1]=='\n')
            {
                m_read_buf[m_checked_idx++]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            //语法错误
            return LINE_BAD;
        }
        else if(temp=='\n'){
            if(m_checked_idx>1&&m_read_buf[m_checked_idx-1]=='\r'){
                m_read_buf[m_checked_idx-1]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;

}
HttpConn::HTTP_CODE HttpConn::parse_request_line(char *text){
    //LOG_INFO("INTO parse_request_line\n");
    //请求行各部分之间通过\t或空格分隔
    m_url=strpbrk(text," \t");
    if(!m_url){
        return BAD_REQUEST;
    }
    *m_url++ ='\0';
    char *method=text;
    if(strcasecmp(method,"GET")==0){
        m_method=GET;
    }else if(strcasecmp(method,"POST")==0){
        m_method=POST;
        cgi=1;
    }else{
        return BAD_REQUEST;//目前只支持get,post
    }
    m_url+=strspn(m_url," \t");
    m_version=strpbrk(m_url," \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++='\0';
    m_version+=strspn(m_version," \t");
    //LOG_INFO("parse_request_line;url:%s\n",m_url);
    if(strcasecmp(m_version,"HTTP/1.1")!=0){//仅支持HTTP/1.1
        return BAD_REQUEST;
    }

    if(strncasecmp(m_url,"http://",7)==0){
        m_url+=7;
        m_url=strchr(m_url,'/');
    }
    if(strncasecmp(m_url,"https://",8)==0){
        m_url+=8;
        m_url=strchr(m_url,'/');
    }

    if(!m_url||m_url[0]!='/'){
        return BAD_REQUEST;
    }

    //如果url只有‘/’，显示欢迎界面
    if(strlen(m_url)==1){
        strcat(m_url,"judge.html");
    }

    //主状态机状态转移
    m_check_state=CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::parse_header(char *text){
    //LOG_INFO("INTO parse_header\n");
    //当前是空行
    if(text[0]=='\0'){
        if(m_content_length!=0){
            m_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    //处理请求头
    else if(strncasecmp(text,"Connection:",11)==0){
        text+=11;
        text+=strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0){
            m_linger=true;
        }
    }
    else if(strncasecmp(text,"Content-length:",15)==0){
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text);
    }
    else if(strncasecmp(text,"Host:",5)==0){
        text+=5;
        text+=strspn(text," \t");
        m_host=text;
    }
    else{
        LOG_INFO("oop!unknown header: %s",text);
    }
    return NO_REQUEST;
}
HttpConn::HTTP_CODE HttpConn::parse_content(char *text){
    if((m_content_length+m_checked_idx)<=m_read_idx){
        text[m_content_length]='\0';
        m_string=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
HttpConn::HTTP_CODE HttpConn::do_request(){
    //LOG_INFO("INTO do_request\n");
    strcpy(m_real_file,doc_root);
    int len=strlen(doc_root);
    const char *p=strrchr(m_url,'/');
    if(cgi==1&&(*(p+1)=='2'||*(p+1)=='3'))
    {
        /*
        处理cgi
        /2CGISQL.cgi
        /3CGISQL.cgi
        */
        //char flag=m_url[1];

        char *m_url_real=(char*)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/");
        strcat(m_url_real,m_url+2);
        strncpy(m_real_file+len,m_url_real,FILENAME_LEN-len-1);
        free(m_url_real);

        //user=123&password=123
        char name[100],password[100];
        int i;
        for(i=5;m_string[i]!='&';++i)
            name[i-5]=m_string[i];
        name[i-5]='\0';
        int j=0;
        for(i=i+10;m_string[i]!='\0';++i,++j)
            password[j]=m_string[i];
        password[j]='\0';

        //注册
        if(*(p+1)=='3'){
            char *sql_insert=(char *)malloc(sizeof(char)*200);
            strcpy(sql_insert,"INSERT INTO user(username,passwd) VALUES(");
            strcat(sql_insert,"'");
            strcat(sql_insert,name);
            strcat(sql_insert,"', '");
            strcat(sql_insert,password);
            strcat(sql_insert,"')");

            if(users.find(name)==users.end()){
                m_locker.lock();
                int res=mysql_query(mysql,sql_insert);
                users.insert(pair<string,string>(name,password));
                m_locker.unlock();

                if(!res){
                    strcpy(m_url,"/log.html");
                }else{
                    strcpy(m_url,"/registerError.html");
                }
            }else{
                strcpy(m_url,"/registerError.html");
            }
        }//登录
        else if(*(p+1)=='2'){
            if(users.find(name)!=users.end()&&users[name]==password)
                strcpy(m_url,"/welcome.html");
            else
                strcpy(m_url,"/logError.html");
        }
    }
    //"/0"表示跳转注册页面
    if(*(p+1)=='0'){
        char *m_url_real=(char *)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/register.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));

        free(m_url_real);
    }
    //"/1"表示跳转登录页面
    else if(*(p+1)=='1'){
        char *m_url_real=(char *)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/log.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p+1)=='5'){
        char *m_url_real=(char *)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/picture.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p+1)=='6'){
        char *m_url_real=(char *)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/video.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p+1)=='7'){
        char *m_url_real=(char *)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/fans.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));

        free(m_url_real);
    }
    else{
        strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);
    }

    //文件不存在
    if(stat(m_real_file,&m_file_stat)<0){
        LOG_INFO("do_request():file don't exit\n");
        return NO_RESOURCE;
    }
        
    //权限不可读
    if(!(m_file_stat.st_mode&S_IROTH))
        return FORBIDDEN_REQUEST;
    //是目录
    if(S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    
    int fd=open(m_real_file,O_RDONLY);
    m_file_address=(char *)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}
bool HttpConn::process_write(HTTP_CODE ret){
    //LOG_INFO("INTO process_write\n");
    switch(ret){
        case INTERNAL_ERROR:
        {
            add_status_line(500,error_500_title);
            add_header(strlen(error_500_form));
            if(!add_content(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(404,error_404_title);
            add_header(strlen(error_404_form));
            if(!add_content(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403,error_403_title);
            add_header(strlen(error_403_form));
            if(!add_content(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200,ok_200_title);
            if(m_file_stat.st_size!=0){
                add_header(m_file_stat.st_size);
                //第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
                m_iv[0].iov_base=m_write_buf;
                m_iv[0].iov_len=m_write_idx;
                //第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
                m_iv[1].iov_base=m_file_address;
                m_iv[1].iov_len=m_file_stat.st_size;
                m_iv_count=2;
                //发送的全部数据是响应报文头部信息和文件大小
                bytes_to_send=m_write_idx+m_file_stat.st_size;
                return true;
            }else{//请求的资源大小为0，则返回空白html文件
                const char *ok_string="<html><body></body></html>";
                add_header(strlen(ok_string));
                if(!add_content(ok_string))
                    return false;

            }
            break;
        }
        default:
        {
            return false;
        }
        //除FILE_REQUEST外，其余状态只申请一个iovec,指向响应报文缓冲区
        m_iv[0].iov_base=m_write_buf;
        m_iv[0].iov_len=m_write_idx;
        m_iv_count=1;
        bytes_to_send=m_write_idx;
        return true;
    }
}
bool HttpConn::add_response(const char *format, ...){
    if(m_write_idx>=WRITE_BUFFER_SIZE)
        return false;
    
    va_list arg_list;
    va_start(arg_list,format);

    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-m_write_idx-1,format,arg_list);

    if(len>=(WRITE_BUFFER_SIZE-m_write_idx-1)){
        va_end(arg_list);
        return false;
    }
    m_write_idx+=len;
    va_end(arg_list);

    LOG_INFO("request:%s",m_write_buf);

    return true;
}
bool HttpConn::add_status_line(int status,const char *title){
    //LOG_INFO("INTO add_status_line;status:%d,title:%s\n",status,title);
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}
bool HttpConn::add_header(int content_length){
    return add_content_length(content_length)&&add_linger()&&
            add_blank_line();
}
bool HttpConn::add_content(const char *content){
    //LOG_INFO("INTO add_header;content:%s\n",content);
    return add_response("%s",content);
}
bool HttpConn::add_content_length(int content_length){
    //LOG_INFO("INTO add_header;content_length:%d\n",content_length);
    return add_response("Content-Length:%d\r\n",content_length);
}
bool HttpConn::add_linger(){
    //LOG_INFO("INTO add_header;add_linger\n");
    return add_response("Connection:%s\r\n",(m_linger==true)?"keep-alive":"close");
}
bool HttpConn::add_blank_line(){
    return add_response("%s","\r\n");
}
bool HttpConn::add_content_type(){
    //LOG_INFO("INTO add_header;add_content_type\n");
    return add_response("Content-Type:%s\r\n","text/html");
}

void HttpConn::unmap(){
    if(m_file_address){
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address=0;
    }
}

void HttpConn::close_conn(bool real_close){
    //LOG_INFO("CLOSE HTTPCONNECTION,sockfd:%d\n",m_sockfd);
    if(real_close&&(m_sockfd!=-1)){
        printf("close sockfd %d\n",m_sockfd);
        removefd(m_epollfd,m_sockfd);
        //修改，删除定时器

        m_sockfd=-1;
        m_user_count--;
    }
}
void HttpConn::process(){
    //LOG_INFO("INTO process\n");
    HTTP_CODE read_ret=process_read();
    if(read_ret==NO_REQUEST){
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
        return;
    }

    bool write_ret=process_write(read_ret);
    if(!write_ret)
    {
        close_conn();
    }

    modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMode);
}
bool HttpConn::read_once(){
    //LOG_INFO("into read_once;use recv() func\n");
    if(m_read_idx>=READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read=0;
    //LT
    if(m_TRIGMode==0){
        bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        m_read_idx+=bytes_read;

        if(bytes_read<=0){
            return false;
        }
        return true;
    }
    //ET
    else{
        while(true){
            bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
            if(bytes_read==-1){
                if(errno==EAGAIN||errno==EWOULDBLOCK)
                    break;
                return false;
            }
            else if(bytes_read==0){//对方已关闭
                return false;
            }
            m_read_idx+=bytes_read;
        }
        return true;
    }
}
bool HttpConn::write(){
    //LOG_INFO("INTO write()\n");
    int temp=0;

    if(bytes_to_send==0){
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
        init();
        return true;
    }
    while(1){
        temp=writev(m_sockfd,m_iv,m_iv_count);
        if(temp<0){
            if(errno==EAGAIN){
                modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }
        bytes_have_send+=temp;
        bytes_to_send-=temp;
        if(bytes_have_send>=m_iv[0].iov_len){
            m_iv[0].iov_len=0;
            m_iv[1].iov_base=m_file_address+(bytes_have_send-m_write_idx);
            m_iv[1].iov_len=bytes_to_send;
        }
        else{
            m_iv[0].iov_base=m_write_buf+bytes_have_send;
            m_iv[0].iov_len=m_iv[0].iov_len-bytes_have_send;
        }

        if(bytes_to_send<=0){
            unmap();
            modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
            if(m_linger){
                init();
                return true;
            }else{
                return false;
            }
        }
    }
}
void HttpConn::init_mysql_map(SqlConnPool *sqlconnpool){
    //LOG_INFO("INTO HttpConn::init_mysql_map\n");
    MYSQL *mysql=NULL;
    SqlConnectionRAII mysqlconn(&mysql,sqlconnpool);//here

    if(mysql_query(mysql,"SELECT username,passwd FROM user")){
        LOG_ERROR("SELECT error:%s\n",mysql_error(mysql));
    }

    MYSQL_RES *result=mysql_store_result(mysql);

    int num_fields=mysql_num_fields(result);
    MYSQL_FIELD *fields=mysql_fetch_fields(result);

    while(MYSQL_ROW row=mysql_fetch_row(result)){
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1]=temp2;
    }

    //m_users=users;
    
}



