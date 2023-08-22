#ifndef THREADPOOL_H
#define THREADPOOL_H
#include<list>
#include<exception>
#include"../Locker/Locker.h"
#include"../Sem/Sem.h"
#include"../SqlConnPool/SqlConnPool.h"
template<typename T>
class ThreadPool {
public:
	ThreadPool(int actor_model,SqlConnPool *sqlConnPool, int thread_number=8, int queue_number=10000);
	~ThreadPool();
	bool append_p(T* request);
	bool append(T *request,int state);
private:
	static void* work(void* arg);
	void run();
private:
	pthread_t* m_threads;
	int m_max_thread_number;
	std::list<T*> m_request_queue;
	int m_max_queue_number;
	Locker m_queue_locker;
	Sem m_queue_sem;

	SqlConnPool *m_sqlConnPool;
	int m_actor_model;


};

template<typename T>
ThreadPool<T>::ThreadPool(int actor_model,SqlConnPool *sqlConnPool, int thread_number, int queue_number):m_actor_model(actor_model),m_sqlConnPool(sqlConnPool),m_max_thread_number(thread_number),m_max_queue_number(queue_number),m_threads(NULL) {
	if (thread_number <=0 || queue_number <=0) {
		throw std::exception();
	}
	m_threads = new pthread_t[m_max_thread_number];
	if (!m_threads) {
		throw std::exception();
	}
	for (int i = 0;i < thread_number;i++) {
		if (pthread_create(m_threads + i, NULL, work, this)!=0) {
			delete[] m_threads;
			throw std::exception();
		}
		if (pthread_detach(m_threads[i])) {
			delete[] m_threads;
			throw std::exception();
		}
	}
}
template<typename T>
ThreadPool<T>::~ThreadPool() {
	delete[] m_threads;
}
template<typename T>
bool ThreadPool<T>::append_p(T* request) {
	m_queue_locker.lock();
	if (m_request_queue.size() >= m_max_queue_number) {
		m_queue_locker.unlock();
		return false;
	}
	m_request_queue.push_back(request);
	m_queue_locker.unlock();
	m_queue_sem.post();
	return true;
}
template<typename T>
bool ThreadPool<T>::append(T *request,int state){
	m_queue_locker.lock();
	if(m_request_queue.size()>=m_max_queue_number){
		m_queue_locker.unlock();
		return false;
	}	
	request->m_state=state;
	m_request_queue.push_back(request);
	m_queue_locker.unlock();
	m_queue_sem.post();
	return true;
}

template<typename T>
void* ThreadPool<T>::work(void* arg) {
	ThreadPool* threadPool = (ThreadPool*)arg;
	threadPool->run();
	return threadPool;
}
//noncompleted
template<typename T>
void ThreadPool<T>::run() {
	while (true) {
		m_queue_sem.wait();
		m_queue_locker.lock();

		if (m_request_queue.empty()) {
			m_queue_locker.unlock();
			continue;
		}
		T* request = m_request_queue.front();
		m_request_queue.pop_front();
		m_queue_locker.unlock();
		if (!request) {
			continue;
		}
		//reactor
		if(1==m_actor_model){
			if(0==request->m_state){//read
				if(request->read_once()){
					request->improv=1;
					SqlConnectionRAII mysqlcon(&request->mysql,m_sqlConnPool);
					request->process();
				}
				else{
					request->improv=1;
					request->timer_flag=1;
				}
			}
			else{//write
				if(request->write()){
					request->improv=1;
				}
				else{
					request->improv=1;
					request->timer_flag=1;
				}
			}
		}
		//proactor
		else{
			SqlConnectionRAII mysqlcon(&request->mysql,m_sqlConnPool);
			request->process();//逻辑代码，不涉及io操作
		}

	}
}
#endif