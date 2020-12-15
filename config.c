#include <stddef.h>

#include "config.h"

char *SUDONAME = "/usr/bin/sudo";
char *WTMPLOGGERNAME = "/usr/local/bin/wtmplogger";

#define SSHDNAME "/usr/sbin/sshd"

/*
 * This structure describes parents that sudo executed wtmplogger process must
 * have. We permit using wtmplogger only when it is used to log an sftp login
 * event.
 *
 * The immediate parent of wtmplogger is in the beginning of the list (sudo).
 * The last parent is the login sshd process at the end of the list.
 * The list is terminated with {.name = NULL} entry.
 */
struct process_check PARENT_PROCESS_CHECK_LIST[] = {
	{.name = "/usr/bin/sudo", .euid = -1 },
	{.name = "/usr/local/bin/sftpwrapper", .euid = -1},
	{.name = SSHDNAME, .euid = -1 },
	{.name = SSHDNAME, .euid = 0 },
	{.name = NULL},
};
