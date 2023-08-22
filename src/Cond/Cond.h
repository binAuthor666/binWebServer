#ifndef COND_H
#define COND_H
#include<exception>
#include<pthread.h>
class Cond {
public:
	Cond() {
		if (pthread_cond_init(&m_cond,NULL) != 0)
		{
			throw std::exception();
		}
	}
	~Cond() {
		pthread_cond_destroy(&m_cond);
	}
	bool wait(pthread_mutex_t* mutex) {
		int ret = 0;
		ret=pthread_cond_wait(&m_cond, mutex);
		return ret==0;
	}
	bool timewait(pthread_mutex_t* mutex, struct timespec t)
	{
		int ret = 0;
		ret = pthread_cond_timedwait(&m_cond, mutex, &t);
		return ret==0;
	}
	bool signal()
	{
		return pthread_cond_signal(&m_cond) == 0;
	}
	bool broadcast()
	{
		return pthread_cond_broadcast(&m_cond) == 0;
	}

private:
	pthread_cond_t m_cond;
};
#endif
