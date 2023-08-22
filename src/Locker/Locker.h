#ifndef LOCKER_H
#define LOCKER_H
#include<exception>
#include<pthread.h>
class Locker {
public:
	Locker() {
		if (pthread_mutex_init(&m_locker, NULL) != 0)
		{
			throw std::exception();
		}
	}
	~Locker() {
		pthread_mutex_destroy(&m_locker);
	}
	bool lock() {
		return pthread_mutex_lock(&m_locker) == 0;
	}
	bool unlock() {
		return pthread_mutex_unlock(&m_locker) == 0;
	}
	pthread_mutex_t* get() {
		return &m_locker;
	}

private:
	pthread_mutex_t m_locker;
};
#endif