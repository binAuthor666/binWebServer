#include"Utils.h"

void Utils::timer_handler(){
    m_time_wheel.tick();
    alarm(m_TIMESLOT);
}

int Utils::setnonblocking(int fd) {
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
	epoll_event event;
	event.data.fd = fd;

	if (1 == TRIGMode) {
		event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	}
	else{
		event.events = EPOLLIN | EPOLLRDHUP;
	}
	if (one_shot)
		event.events |= EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}
void Utils::sig_handler(int sig) {
	int save_errno = errno;
	int msg = sig;
	send(u_pipefd[1], (char*)&msg, 1, 0);
	errno = save_errno;
}
void Utils::add_sig(int sig, void(handler)(int), bool restart) {
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	if (restart) {
		sa.sa_flags |= SA_RESTART;
	}

	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}


void Utils::show_error(int connfd, const char* info)
{
	send(connfd, info, strlen(info), 0);
	close(connfd);
}

int* Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

void cb_func(ClientData* client_data) {
	printf("timer want to close sockfd:%d\n",client_data->sockfd);
	fflush(stdout);
	epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, client_data->sockfd, 0);
	assert(client_data);
	close(client_data->sockfd);
	HttpConn::m_user_count--;
}