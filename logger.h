#ifndef _SFTPWRAPPER_LOGGER_H_
#define _SFTPWRAPPER_LOGGER_H_

#include <stdio.h>
#include <syslog.h>

#define log_error(fmt, args...) do { \
	fprintf(stderr, "error: %s %s:%d: " fmt, \
		__FILE__, __func__, __LINE__, ## args); \
	syslog(LOG_ERR, "error: %s %s:%d: " fmt, \
		__FILE__, __func__, __LINE__, ## args); \
} while(0)

#define log_fatal(fmt, args...) do { \
	fprintf(stderr, "fatal error: %s %s:%d: " fmt, \
		__FILE__, __func__, __LINE__, ## args); \
	syslog(LOG_ERR, "fatal error: %s %s:%d: " fmt, \
		__FILE__, __func__, __LINE__, ## args); \
	abort(); \
} while(0)

#endif
