#ifndef UTILS_H
#define UTILS_H
#include "TimeWheel.h"
#include"../Http/HttpConn.h"
class Utils{
public:
    Utils(){}
	~Utils(){}

	void init(){
        m_TIMESLOT=TimeWheel::SI;
    }
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
	TimeWheel m_time_wheel; 
};
void cb_func(ClientData* client_data);
#endif