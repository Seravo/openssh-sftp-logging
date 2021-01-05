/*
 * A command wrapper to log sftp sessions into wtmp on GNU/Linux.
 *
 * Copyright 2020 Seravo Oy
 *
 * SPDX short identifier: BSD-2-Clause
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include "config.h"
#include "logger.h"
#include "proc_pid_util.h"

static const char *DOC_STRING =
"A command wrapper to log sftp sessions into wtmp on GNU/Linux.\n"
"\n"
"USAGE:\n"
"\n"
"Change the following line in sshd_config from:\n"
"\n"
"    Subsystem sftp /usr/lib/openssh/sftp-server\n"
"\n"
"to:\n"
"\n"
"    Subsystem sftp /usr/local/bin/sftpwrapper -c SSH_CLIENT -- /usr/lib/openssh/sftp-server\n"
"\n"
"BUGS AND LIMITATIONS:\n"
"\n"
"* Hardcoded executable paths for sudo and wtmplogger.\n"
"  These should be made configurable.\n"
"\n"
"* Hardcoded parent process checking in wtmplogger.\n"
"  These should be made configurable.\n"
"\n"
"* Only supports Linux /proc file system to get process information.\n"
"\n"
"AUTHOR\n"
"\n"
"You can mail feedback and improvements to Heikki Orsila <heikki.orsila@iki.fi>\n";

#define VERSION "0.0.1"


static void call_wtmplogger(const char *ut_type, const char *no_tty,
			    const char *host)
{
	int wstatus;
	char ut_pid[64];
	pid_t child;
	/* Get parent's parent process, and use it as a login process */
	pid_t login_pid = get_process_parent_pid(getppid());

	snprintf(ut_pid, sizeof(ut_pid), "%d", (int) login_pid);

	child = fork();
	if (child == 0) {
		execl(SUDONAME, SUDONAME, WTMPLOGGERNAME,
		      ut_type, ut_pid, no_tty, host, (char *) NULL);
		log_fatal("execl %s %s failed\n", SUDONAME, WTMPLOGGERNAME);
	}

	waitpid(child, &wstatus, 0);
	if (WIFEXITED(wstatus)) {
		int ret = WEXITSTATUS(wstatus);
		if (ret != 0) {
			log_error("%s %s failed: ret = %d",
				  SUDONAME, WTMPLOGGERNAME, ret);
		}
	} else {
		log_error("%s %s died", SUDONAME, WTMPLOGGERNAME);
	}
}

static void parse_host(char **host, const char *var)
{
	/*
	 * Note: we don't abort the program if there is a problem determining
	 * the host, because it is more important to serve than to log.
	 */
	char *env = getenv(var);
	if (env == NULL) {
		log_error("%s env variable does not exist\n", var);
		return;
	}
	int ret = sscanf(env, "%ms", host);
	if (ret != 1) {
		log_error("Unable to parse host\n");
		return;
	}
}

static int parse_options(char **notty, char **host, int argc, char *argv[])
{
	while (1) {
		int c;
		int option_index = 0;
		static struct option long_options[] = {
			{"client", required_argument, 0, 'c'},
			{"help", no_argument, 0, 'h'},
			{"no-tty", required_argument, 0, 'n'},
			{"version", no_argument, 0, 'v'},
			{0, 0, 0, 0},
		};

		c = getopt_long(argc, argv, "c:hn:v", long_options,
				&option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'c':
			parse_host(host, optarg);
			break;
		case 'h':
			fwrite(DOC_STRING, strlen(DOC_STRING), 1, stdout);
			exit(0);
		case 'n':
			*notty = strdup(optarg);
			if (*notty == NULL) {
				/* Note: we don't fail for this */
				log_error("No memory for no-tty\n");
				*notty = SFTPWRAPPER_DEFAULT_NOTTY;
			}
			break;
		case 'v':
			printf("Version %s\n", VERSION);
			exit(0);
		case '?':
			log_error("Unknown option %c\n", optopt);
			exit(1);
		default:
			log_error("getopt returned character code 0%o\n", c);
			exit(1);
		}
	}

	return optind;
}


int main(int argc, char *argv[])
{
	int wstatus;
	int ret = EXIT_SUCCESS;
	pid_t child;
	int exec_ind;
	/* What to log when there is no tty */
	char *no_tty = SFTPWRAPPER_DEFAULT_NOTTY;
	char *host = NULL;  /* Which host to log */

	exec_ind = parse_options(&no_tty, &host, argc, argv);

	call_wtmplogger("USER_PROCESS", no_tty, host);

	if (exec_ind < argc) {
		child = fork();
		if (child == 0) {
			execv(argv[exec_ind], &argv[exec_ind]);
			log_error("Could not execute %s (%s)\n",
				  argv[exec_ind], strerror(errno));
			abort();
		}
		waitpid(child, &wstatus, 0);
		if (WIFEXITED(wstatus)) {
			ret = WEXITSTATUS(wstatus);
		} else {
			ret = EXIT_FAILURE;
		}
	}
	
	call_wtmplogger("DEAD_PROCESS", no_tty, NULL);

	return ret;
}
