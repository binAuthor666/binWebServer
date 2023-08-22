#ifndef CONFIG_H
#define CONFIG_H
#include"Webserver.h"
class Config{
public:
    Config();
    ~Config(){}
    void parse_arg(int argc,char *argv[]);

    int PORT;
    int LOGWRITE;
    int TRIGMODE;
    int LISTENTRIGMODE;
    int CONNTRIGMODE;
    int OPTLINGER;
    int SQLNUM;
    int THREADNUM;
    int CLOSELOG;
    int ACTORMODEL;
};
#endif