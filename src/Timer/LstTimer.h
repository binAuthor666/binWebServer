#ifndef LSTTIMER_H
#define LSTTIMER_H

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
#include "../Log/Log.h"
class ClassTimer;
struct ClientData {
	sockaddr_in addr;
	int sockfd;
	ClassTimer* timer;
};

class ClassTimer {
public:
	ClassTimer():prev(NULL),next(NULL){}
public:
	time_t expire;
	ClientData *client_data;
	ClassTimer*prev;
	ClassTimer*next;
	void(*cb_func)(ClientData *);

};

class SortTimerLst {
public:
	SortTimerLst();
	~SortTimerLst();

	void add_timer(ClassTimer* timer);
	void del_timer(ClassTimer* timer);
	void adjust_timer(ClassTimer* timer);
	void tick();
private:
	void add_timer(ClassTimer* timer, ClassTimer* lst_head);
	ClassTimer* head;
	ClassTimer* tail;
};

class Utils {
public:
	Utils(){}
	~Utils(){}

	void init(int timeslot);
	int setnonblocking(int fd);
	void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

	static void sig_handler(int sig);
	void add_sig(int sig, void(handler)(int), bool restart = true);
	void timer_handler();

	void show_error(int connfd, const char* info);
public:
	static int u_epollfd;
	static int *u_pipefd;
	int m_TIMESLOT;
	SortTimerLst m_timer_lst;
};

void cb_func(ClientData* client_data);
#endif