#ifndef TWTIMER_H
#define TWTIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
class TWTimer;
class ClientData{
public:
    sockaddr_in addr;
    int sockfd;
    TWTimer* timer;
};

class TWTimer{
public:
    TWTimer(int rot,int slot):prev(nullptr),next(nullptr),rotation(rot),time_slot(slot),cb_func(nullptr){}

public:
    int rotation;//转多少圈
    int time_slot;//时间槽
    TWTimer *prev;
    TWTimer *next;
    ClientData *clientData;
    void(*cb_func)(ClientData*);
};


#endif