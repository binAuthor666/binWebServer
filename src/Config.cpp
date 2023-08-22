#include"Config.h"

Config::Config(){
    PORT=9006;
    LOGWRITE=0;//default:synchron
    TRIGMODE=0;//defaukt:listenfd LT +connfd LT
    LISTENTRIGMODE=0;
    CONNTRIGMODE=0;
    OPTLINGER=0;//default:non
    SQLNUM=8;
    THREADNUM=8;
    CLOSELOG=0;//default:open
    ACTORMODEL=0;//default:proactor
}

void Config::parse_arg(int argc,char *argv[]){
    int opt;
    const char *str="p:l:m:o:s:t:c:a:";

    while(opt=getopt(argc,argv,str)!=-1){
        switch(opt){ 
            case 'p':
            {
                PORT=atoi(optarg);
                break;
            }
            case 'l':
            {
                LOGWRITE=atoi(optarg);
                break;
            }
            case 'm':
            {
                TRIGMODE=atoi(optarg);
                break;
            }
            case 'o':
            {
                OPTLINGER=atoi(optarg);
                break;
            }
            case 's':
            {
                SQLNUM=atoi(optarg);
                break;
            }
            case 't':
            {
                THREADNUM=atoi(optarg);
                break;
            }
            case 'c':
            {
                CLOSELOG=atoi(optarg);
                break;
            }
            case 'a':
            {
                ACTORMODEL=atoi(optarg);
                break;
            }
            default:
            break;
        }
    }
}