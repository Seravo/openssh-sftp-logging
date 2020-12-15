#ifndef _SFTPWRAPPER_PROC_PID_UTIL_H_
#define _SFTPWRAPPER_PROC_PID_UTIL_H_

#include <string.h>
#include <sys/types.h>

uid_t get_process_euid(const pid_t pid);
pid_t get_process_parent_pid(const pid_t pid);
int get_process_exe(char *exe_name, const size_t bufsize, const pid_t pid);

#endif
