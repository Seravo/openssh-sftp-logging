/*
 * Write an entry into utmp and wtmp based on cmdline arguments in a
 * specific OpenSSH sftp-server context.
 *
 * SYNTAX: wtmplogger ut_type ut_pid no_tty_line [ut_host]
 *
 * - ut_type is a string: USER_PROCESS or DEAD_PROCESS
 * - ut_pid is an integer pid of the login
 * - no_tty_line is what to set as "line" if there is no tty at stdin
 * - ut_host is the remote host to log (optional)
 *
 * See {includepath}/bits/utmp.h for explanation of fields.
 *
 * ERROR HANDLING: Terminates program if anything goes wrong.
 *
 * BUGS: Works only for a specific parent process path.
 *       See check_parent_processes() function. This can be fixed with
 *       a configuration file that specifies the correct path.
 *
 * COMPATIBILITY: Supports only GNU/Linux at the moment.
 *
 * Copyright 2020 Seravo Oy
 *
 * SPDX short identifier: BSD-2-Clause
 *
 * wtmp_write() function is copied with modifications from OpenSSH:
 * https://github.com/openssh/openssh-portable/blob/a5ab499bd2644b4026596fc2cb24a744fa310666/loginrec.c#L1092
 * wtmp_write() is copyrighted as follows:
 *
 * Portions copyright (c) 2000 Andre Lucas
 * Portions copyright (c) 1998 Todd C. Miller
 * Portions copyright (c) 1996 Jason Downs
 * Portions copyright (c) 1996 Theo de Raadt
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include "config.h"
#include "logger.h"
#include "proc_pid_util.h"

/*
 * Write a wtmp entry direct to the end of the file
 */
static void wtmp_write(const struct utmp *ut)
{
	struct stat buf;
	int fd = open(WTMP_FILE, O_WRONLY | O_APPEND, 0);
	if (fd < 0)
		log_fatal("Error when opening %s: %s\n",
			  WTMP_FILE, strerror(errno));
	if (fstat(fd, &buf) == 0) {
		if (write(fd, ut, sizeof(*ut)) != sizeof(*ut)) {
			int ret = ftruncate(fd, buf.st_size);
			(void) ret;
			log_fatal("Error when writing to %s: %s\n",
				  WTMP_FILE, strerror(errno));
		}
	}
	close(fd);
}

static void write_ut(const short ut_type,
		     const pid_t sshd_pid,
		     const char *ut_line,
		     const char *user,
		     const char *host,
		     const struct addrinfo *addrinfo)
{
	struct utmp ut = {
		.ut_type = ut_type,
		.ut_pid = sshd_pid,
		.ut_tv.tv_sec = time(NULL),
	};
	snprintf(ut.ut_line, sizeof(ut.ut_line), "%s", ut_line);
	if (user != NULL)
		snprintf(ut.ut_user, sizeof(ut.ut_name), "%s", user);
	if (host != NULL)
		snprintf(ut.ut_host, sizeof(ut.ut_host), "%s", host);

	if (addrinfo != NULL && addrinfo->ai_family == AF_INET6) {
		const struct sockaddr_in6 *in = (
			(struct sockaddr_in6 *) addrinfo->ai_addr);
		if (sizeof(in->sin6_addr) == sizeof(ut.ut_addr_v6)) {
			memcpy(&ut.ut_addr_v6, in->sin6_addr.s6_addr,
			       sizeof(ut.ut_addr_v6));
			if (IN6_IS_ADDR_V4MAPPED(&in->sin6_addr)) {
				ut.ut_addr_v6[0] = ut.ut_addr_v6[3];
				ut.ut_addr_v6[1] = 0;
				ut.ut_addr_v6[2] = 0;
				ut.ut_addr_v6[3] = 0;
			}
		} else {
			log_fatal("Incompatible address length: %d\n",
				  addrinfo->ai_addrlen);
		}
	}

	setutent();
	pututline(&ut);
	endutent();

	wtmp_write(&ut);
}

static void parse_address(struct addrinfo **addrinfo, const char *host)
{
	/* The address is parsed to find out address family (ipv4 or ipv6) */
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,  /* Allow IPv4 or IPv6 */
		.ai_socktype = SOCK_STREAM,  /* Datagram socket */
	};
	int ret;
	if (host[0] == 0)
		return;
	ret = getaddrinfo(host, NULL, &hints, addrinfo);
	if (ret != 0)
		log_fatal("getaddrinfo error: %s\n", gai_strerror(ret));
}

static void read_args(int *ut_type, pid_t *sshd_pid, char **notty, char **host,
		      const int argc, char *argv[])
{
	int pidint = -1;
	char *ut_type_str = NULL;

	if (argc < 4) {
		log_fatal("Invalid number of command line parameters. "
			  "Need at least 4.\n");
	}

	if (sscanf(argv[1], "%ms", &ut_type_str) != 1)
		log_fatal("Can not read ut_type\n");
	if (strcmp(ut_type_str, "DEAD_PROCESS") == 0) {
		*ut_type = DEAD_PROCESS;
	} else if (strcmp(ut_type_str, "USER_PROCESS") == 0) {
		*ut_type = USER_PROCESS;
	} else {
		log_fatal("Bad value for ut_type: %s\n", ut_type_str);
	}

	if (sscanf(argv[2], "%d", &pidint) != 1)
		log_fatal("Can not read pid\n");
	*sshd_pid = pidint;

	if (sscanf(argv[3], "%ms", notty) != 1)
		log_fatal("Can not read notty\n");

	if (*ut_type == USER_PROCESS && argc >= 5) {
		if (sscanf(argv[4], "%ms", host) != 1)
			log_fatal("Can not read host\n");
	}
}

static void check_parent_processes(void)
{
	/*
	 * HACK HACK
	 *
	 * Verify that the process is called through sudo, and sudo is called
	 * from sftpwrapper, and sftpwrapper is called from sshd, which is
	 * called from sshd owned by root.
	 *
	 * TO DO: Implement configuration file to enforce abtrary parent
	 *        process routes.
	 */
	pid_t pid = getppid();
	pid_t ppid;
	char exe_name[PATH_MAX];
	uid_t euid;
	int i;

	for (i = 0; PARENT_PROCESS_CHECK_LIST[i].name != NULL; i++) {
		if (pid == 0)
			log_fatal("No parent process");

		euid = get_process_euid(pid);
		if (euid == ((uid_t) -1)) {
			log_fatal("Could not get euid for pid %d\n",
				  (int) pid);
		}
		if (PARENT_PROCESS_CHECK_LIST[i].euid != ((uid_t) -1) &&
		    euid != PARENT_PROCESS_CHECK_LIST[i].euid) {
			log_fatal("expected uid %d at step %d\n",
				  PARENT_PROCESS_CHECK_LIST[i].euid, i);
		}

		if (get_process_exe(exe_name, sizeof(exe_name), pid)) {
			log_fatal("Could not determined exe name of pid %d\n",
				  (int) pid);
		}

		if (strcmp(exe_name, PARENT_PROCESS_CHECK_LIST[i].name) != 0) {
			log_fatal("Chain of trust broken: pid %d (exe %s) "
				  "does not match exe %s\n",
				  (int) pid, exe_name,
				  PARENT_PROCESS_CHECK_LIST[i].name);
		}

		ppid = get_process_parent_pid(pid);
		if (ppid == ((pid_t) -1)) {
			log_fatal("Could not get parent process for pid %d\n",
				  (int) pid);
		}
		pid = ppid;
	}
}

static char *define_ut_line(char *notty)
{
	char *tty = ttyname(STDIN_FILENO);
	const char *ttyprefix = "/dev/";
	const size_t ttyprefixlen = strlen(ttyprefix);
	char *ut_line = NULL;

	if (tty == NULL) {
		tty = notty;
	} else {
		if (strncmp(tty, "/dev/", ttyprefixlen) == 0)
			tty += ttyprefixlen;
	}

#ifdef WTMPLOGGER_NO_PARENT_PROCESS_CHECK
	do {
		/*
		 * We can not trust information provided by parent for logging
		 * so we prepend "sftp:" (string) to ut_line to indicate this.
		 */
		const char *prefix = "sftp:";
		size_t ut_line_size;
		int ret;
		ut_line_size = strlen(prefix) + strlen(tty) + 1;
		ut_line = malloc(ut_line_size);
		if (ut_line == NULL)
			log_fatal("No memory for ut_line\n");
		ret = snprintf(ut_line, ut_line_size, "%s%s", prefix, tty);
		if ((size_t) ret >= ut_line_size)
			log_fatal("Too long a string for ut_line\n");
	} while (0);
#else
	ut_line = strdup(tty);
	if (ut_line == NULL)
		log_fatal("No memory for ut_line\n");
#endif

	return ut_line;
}


int main(int argc, char *argv[])
{
	char *ut_line = NULL;
	char *notty = "notty";  /* Which line to log when there is no tty */
	char *host = "";   /* Which host to log by default */
	struct addrinfo *addrinfo = NULL;
	int ut_type = 0;
	pid_t sshd_pid = -1;
	const char *log_name = getenv("SUDO_USER");
	if (log_name == NULL)
		log_fatal("No SUDO_USER env\n");

#ifndef WTMPLOGGER_NO_PARENT_PROCESS_CHECK
	check_parent_processes();
#endif

	read_args(&ut_type, &sshd_pid, &notty, &host, argc, argv);

	ut_line = define_ut_line(notty);

	if (ut_type == USER_PROCESS)
		parse_address(&addrinfo, host);

	write_ut(ut_type, sshd_pid, ut_line, log_name, host, addrinfo);
	return 0;
}
