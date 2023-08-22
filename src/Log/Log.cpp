#include"Log.h"

Log::Log() {
	m_line_cnt = 0;
	m_async_flag = false;
}

Log::~Log() {
	if (m_fp != NULL) {
		fclose(m_fp);
	}
}

bool Log::init(const char* file_name, int close_log, int max_buffer_size, int max_lines, int max_queue_size) {
	if (max_queue_size > 0) {
		m_async_flag = true;
		m_log_queue = new BlockQueue<std::string>[max_queue_size];
		pthread_t tid;
		pthread_create(&tid, NULL, flush_log_thread, NULL);

	}
	m_max_queue_size = max_queue_size;
	m_close_log = close_log;
	m_max_buffer_size = max_buffer_size;
	m_max_lines = max_lines;

	m_buffer = new char[m_max_buffer_size];
	memset(m_buffer, 0, m_max_buffer_size);

	time_t t = time(NULL);
	struct tm* sys_tm = localtime(&t);
	struct tm m_tm = *sys_tm;

	m_today = m_tm.tm_mday;

	const char* p = strrchr(file_name, '/');
	char completed_log_name[256] = { 0 };
	if (p == NULL) {
		snprintf(completed_log_name, 255, "%d_%02d_%02d_%s", m_tm.tm_year + 1900, m_tm.tm_mon + 1, m_tm.tm_mday, file_name);
	}
	else {
		strcpy(log_name, p + 1);
		strncpy(dir_name, file_name, p - file_name + 1);
		snprintf(completed_log_name, 255, "%s%d_%02d_%02d_%s",dir_name, m_tm.tm_year + 1900, m_tm.tm_mon + 1, m_tm.tm_mday, log_name);
	}

	m_fp = fopen(completed_log_name, "a");
	if (m_fp == NULL) {
		return false;
	}
	return true;
}

void Log::format_write(int level, const char* format, ...) {
	struct timeval now = { 0, 0 };
	gettimeofday(&now, NULL);
	time_t t = now.tv_sec;
	struct tm* sys_tm = localtime(&t);
	struct tm m_tm = *sys_tm;

	char s[16] = { 0 };
	switch (level) {
	case 0:
		strcpy(s, "[debug]:");
		break;
	case 1:
		strcpy(s, "[info]:");
		break;
	case 2:
		strcpy(s, "[warn];");
		break;
	case 3:
		strcpy(s, "[erro]:");
		break;
	default:
		strcpy(s, "[info]:");
		break;
	}

	m_locker.lock();
	m_line_cnt++;
	if (m_today != m_tm.tm_mday || m_line_cnt % m_max_lines == 0) {
		char new_log[256] = { 0 };
		fflush(m_fp);
		fclose(m_fp);
		char tail[16] = { 0 };
		snprintf(tail, 16, "%d_%02d_%02d_", m_tm.tm_year + 1900, m_tm.tm_mon + 1, m_tm.tm_mday);
		if (m_today != m_tm.tm_mday) {
			snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
			m_today = m_tm.tm_mday;
			m_line_cnt = 0;
		}
		else {
			snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name,m_line_cnt/m_max_lines);
		}
		m_fp=fopen(new_log, "a");
	}

	m_locker.unlock();

	va_list valist;
	va_start(valist, format);

	std::string log_str;
	m_locker.lock();
	int n=snprintf(m_buffer, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s",
		m_tm.tm_year + 1900, m_tm.tm_mon + 1, m_tm.tm_mday,
		m_tm.tm_hour, m_tm.tm_min, m_tm.tm_sec, now.tv_usec, s);
	int m = vsnprintf(m_buffer + n, m_max_buffer_size - n - 1, format, valist);
	m_buffer[m + n] = '\n';
	m_buffer[m + n + 1] = '\0';
	log_str = m_buffer;

	m_locker.unlock();
	if (m_async_flag && !m_log_queue->full()) {
		m_log_queue->push(log_str);
	}
	else {
		m_locker.lock();
		fputs(log_str.c_str(), m_fp);
		m_locker.unlock();
	}
	va_end(valist);

}

void Log::flush(void) {
	m_locker.lock();
	fflush(m_fp);
	m_locker.unlock();
}

