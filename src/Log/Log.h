#ifndef LOG_H
#define LOG_H
#include<stdio.h>
#include<iostream>
#include<string.h>
#include<stdarg.h>
#include<pthread.h>
#include"BlockQueue.h"


class Log
{
public:
	static Log* getInstance() {
		static Log instance;
		return &instance;
	}

	bool init(const char* file_name, int close_log, int max_buffer_size=8192, int max_lines=5000000, int max_queue_size = 0);

	static void* flush_log_thread(void* arg) {
		Log::getInstance()->async_write_log();
	}

	void format_write(int level, const char* format, ...);

	void flush(void);
private:
	Log();
	virtual ~Log();
	void* async_write_log() {
		std::string single_log;
		while (m_log_queue->pop(single_log)) {
			m_locker.lock();
			fputs(single_log.c_str(), m_fp);
			m_locker.unlock();
		}
	}

private:
	char dir_name[128];
	char log_name[128];
	char* m_buffer;
	int m_max_buffer_size;
	int m_max_lines;
	long long m_line_cnt;
	int m_today;
	FILE* m_fp;
	BlockQueue<std::string> *m_log_queue;
	int m_max_queue_size;
	Locker m_locker;
	int m_close_log; 
	bool m_async_flag; 

};
#define LOG_DEBUG(format, ...) if(0==m_close_log){Log::getInstance()->format_write(0,format,#__VA_ARGS__);Log::getInstance()->flush();}
#define LOG_INFO(format, ...) if(0==m_close_log){Log::getInstance()->format_write(1,format,#__VA_ARGS__);Log::getInstance()->flush();}
#define LOG_WARN(format, ...) if(0==m_close_log){Log::getInstance()->format_write(2,format,#__VA_ARGS__);Log::getInstance()->flush();}
#define LOG_ERROR(format, ...) if(0==m_close_log){Log::getInstance()->format_write(3,format,#__VA_ARGS__);Log::getInstance()->flush();}


#endif // !LOG_H
