#ifndef _SFTPWRAPPER_CONFIG_H_
#define _SFTPWRAPPER_CONFIG_H_

#include <sys/types.h>

#define SFTPWRAPPER_DEFAULT_NOTTY "sftp"

struct process_check {
	char *name;
	uid_t euid;
};

extern struct process_check PARENT_PROCESS_CHECK_LIST[];

extern char *SUDONAME;
extern char *WTMPLOGGERNAME;

#endif
