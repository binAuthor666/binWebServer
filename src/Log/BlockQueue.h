#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H
#include<sys/time.h>
#include<stdlib.h>
#include"../Locker/Locker.h"
#include"../Cond/Cond.h"
template<typename T>
class BlockQueue {
public:
	BlockQueue(int max_size = 1000) {
		if (max_size <= 0)
		{
			exit(-1);
		}
		m_max_size = max_size;
		m_size = 0;
		m_array = new T[max_size];
		m_front = -1;
		m_back = -1;
	}
	~BlockQueue() {
		m_locker.lock();
		if (m_array != NULL)
			delete[] m_array;
		m_locker.unlock();
	}
	void clear() {
		m_locker.lock();
		m_size = 0;
		m_front = -1;
		m_back = -1;
		m_locker.unlock();
	}
	bool full() {
		m_locker.lock();
		if (m_size >= m_max_size)
		{
			m_locker.unlock();
			return true;
		}
		m_locker.unlock();
		return false;
	}
	bool empty() {
		m_locker.lock();
		if (0 == m_size)
		{
			m_locker.unlock();
			return true;
		}
		m_locker.unlock();
		return false;
	}
	bool front(T& value) {
		m_locker.lock();
		if (0 == m_size) 
		{
			m_locker.unlock();
			return false;
		}
		value = m_array[m_front];
		m_locker.unlock();
		return true;
	}
	bool back(T& value) {
		m_locker.lock();
		if (0 == m_size)
		{
			m_locker.unlock();
			return false;
		}
		value = m_array[m_back];
		m_locker.unlock();
		return true;
	}
	int size() {
		int tmp = 0;
		m_locker.lock();
		tmp = m_size;
		m_locker.unlock();
		return tmp;
	}
	int max_size() {
		int tmp = 0;
		m_locker.lock();
		tmp = m_max_size;
		m_locker.unlock();
		return tmp;
	}
	bool push(const T& item) {
		m_locker.lock();
		if (m_size >= m_max_size) {
			m_cond.broadcast();
			m_locker.unlock();
			return false;
		}
		m_back = (m_back + 1) % m_max_size;
		m_array[m_back] = item;
		++m_size;

		m_cond.broadcast();
		m_locker.unlock();
		return true;
	}
	bool pop(T &item) {
		m_locker.lock();
		while (m_size <= 0) {
			if (!m_cond.wait(m_locker.get())) {
				m_locker.unlock();
				return false;
			}
		}
		m_front = (m_front + 1) % m_max_size;
		item = m_array[m_front];
		
		m_size--;
		m_locker.unlock();
		return true;
	}
	bool pop(T& item, int ms_timeout)
	{
		struct timespec t = { 0,0 };
		struct timeval now = { 0,0 };
		gettimeofday(&now, NULL);
		m_locker.lock();
		if (m_size <= 0)
		{
			t.tv_sec = now.tv_sec + ms_timeout / 1000;
			t.tv_nsec = (ms_timeout % 1000) * 1000;
			if (!m_cond.timewait(m_locker.get(), t))
			{
				m_locker.unlock();
				return false;
			}
		}
		if (m_size <= 0)
		{
			m_locker.unlock();
			return false;
		}
		m_front = (m_front + 1) % m_max_size;
		item = m_array[m_front];
		
		m_size--;
		m_locker.unlock();
		return true;
	}
private:
	Locker m_locker;
	Cond m_cond;

	T* m_array;
	int m_max_size;
	int m_size;
	int m_front;
	int m_back;
};
#endif