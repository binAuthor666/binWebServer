#include"LstTimer.h"
#include"../Http/HttpConn.h"

SortTimerLst::SortTimerLst() {
	head = NULL;
	tail = NULL;
}

SortTimerLst::~SortTimerLst() {
	ClassTimer* tmp = head;
	while (tmp) {
		head = tmp->next;
		delete tmp;
		tmp = head;
	}
}

void SortTimerLst::add_timer(ClassTimer* timer) {
	if (!timer) {
		return;
	}
	if (!head) {
		head = tail = timer;
		return;
	}
	if (timer->expire < head->expire) {
		timer->next = head;
		head->prev = timer;
		head = timer;
		return;
	}
	add_timer(timer, head);
}
void SortTimerLst::del_timer(ClassTimer* timer) {
	if (!timer) return;
	if (timer == head && timer == tail) {
		delete timer;
		head = NULL;
		tail = NULL;
		return;
	}
	if (timer == head) {
		head = head->next;
		head->prev = NULL;
		delete timer;
		return;
	}
	if (timer == tail) {
		tail = tail->prev;
		tail->next = NULL;
		delete timer;
		return;
	}
	timer->prev->next = timer->next;
	timer->next->prev = timer->prev;
	delete timer;
}
void SortTimerLst::adjust_timer(ClassTimer* timer) {
	if (!timer) {
		return;
	}
	ClassTimer* tmp = timer->next;
	if (!tmp || timer->expire < tmp->expire) {
		return;
	}
	if (timer == head) {
		head = head->next;
		head->prev = NULL;
		timer->next=NULL;
		add_timer(timer, head);
	}
	else {
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		add_timer(timer, timer->next);
	}

}
void SortTimerLst::tick() {
	if (!head) return;
	time_t cur = time(NULL);
	ClassTimer* tmp = head;
	while (tmp) {
		if (cur < tmp->expire) {
			break;
		}
		tmp->cb_func(tmp->client_data);

		head = tmp->next;
		if (head) {
			head->prev = NULL;
		}

		delete tmp;
		tmp = head;
	}
}

void SortTimerLst::add_timer(ClassTimer* timer, ClassTimer* lst_head) {
	ClassTimer* prev = lst_head;
	ClassTimer* tmp = prev->next;
	while (tmp) {
		if (timer->expire < tmp->expire) {
			prev->next = timer;
			timer->next = tmp;
			tmp->prev = timer;
			timer->prev = prev;
			break;
		}
		prev = tmp;
		tmp = tmp->next;
	}
	if (!tmp) {
		prev->next = timer;
		timer->prev = prev;
		timer->next = NULL;
		tail = timer;

	}
}



void Utils::init(int timeslot) {
	m_TIMESLOT = timeslot;
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

void Utils::timer_handler() {
	m_timer_lst.tick();
	alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char* info)
{
	send(connfd, info, strlen(info), 0);
	close(connfd);
}

int* Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

void cb_func(ClientData* client_data) {
	epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, client_data->sockfd, 0);
	assert(client_data);
	close(client_data->sockfd);
	HttpConn::m_user_count--;
}