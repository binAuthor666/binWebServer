#include"Config.h"
#include"Webserver.h"
FILE *mainfp;
int main(int argc,char *argv[]){
    
    mainfp=fopen("./mainlog","a");
    std::string user="root";
    std::string password="passwd";
    std::string databasename="bindb";
    Config config;
    config.parse_arg(argc,argv);
    WebServer server;
    server.init(config.PORT,user,password,databasename,config.LOGWRITE,
    config.OPTLINGER,config.TRIGMODE,config.SQLNUM,config.THREADNUM,
    config.CLOSELOG,config.ACTORMODEL);

    server.log_write();

    server.sql_pool();

    server.thread_pool();//here

    server.trig_mode();

    server.event_listen();

    server.event_loop();
    fclose(mainfp);
    return 0;
}
