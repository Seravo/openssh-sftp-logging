/*
 * Linux /proc/pid utility module for sftpwrapper
 *
 * Copyright 2020 Seravo Oy
 *
 * SPDX short identifier: BSD-2-Clause
 */
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "logger.h"
#include "proc_pid_util.h"

/*
 * Get a value from line beginning with attr string in file
 * /proc/pid/status. If index == 0, the first value after
 * attr is returned. If index == 1, the second value is returned.
 * If index is anything else than 0 or 1, returns -1.
 * Returns -1 if anything unexpected happens.
 *
 * Reads /proc/${pid}/status line by line. Scans for attr.
 * When attr is found, returns word at a given index as an integer.
 */
static int read_process_attribute(const pid_t pid, const char *attr,
				  const unsigned index)
{
	char path[PATH_MAX];
	char line[PATH_MAX];
	FILE *f;
	char *field = NULL;
	char *value = NULL;
	int int_value = -1;

	int ret = snprintf(path, sizeof path, "/proc/%d/status", (int) pid);
	if ((size_t) ret >= sizeof(path)) {
		log_error("Unexpected: PATH_MAX exceeded (pid)\n");
		goto error;
	}

	f = fopen(path, "r");
	if (f == NULL) {
		log_error("Unexpected: Can not open %s\n", path);
		goto error;
	}

	while (1) {
		if (fgets(line, sizeof(line), f) == NULL) {
			log_error("Unexpected: %s not found (EOF)\n", attr);
			goto error;
		}

		/* Read 2 or 3 words from the line depending on index */
		if (index == 0) {
			ret = sscanf(line, "%ms %ms", &field, &value);
			if (ret < 2) {
				log_error("Unexpected: less than 2 words "
					  "read\n");
				goto error;
			}
		} else if (index == 1) {
			char *tmp = NULL;
			ret = sscanf(line, "%ms %ms %ms",
				     &field, &tmp, &value);
			free(tmp);
			tmp = NULL;
			if (ret < 2) {
				log_error("Unexpected: less than 2 words "
					  "read\n");
				goto error;
			}
			if (ret != 3)
				continue;
		} else {
			log_error("Invalid index argument: %d\n", index);
			goto error;
		}

		if (strcmp(field, attr) == 0) {
			int_value = atoi(value);
			if (int_value < 0) {
				log_error("Unexpected negative value: %s %s\n",
					  attr, value);
				goto error;
			}
			break;
		}

		free(field);
		free(value);
		field = NULL;
		value = NULL;
	}

	fclose(f);
	free(field);
	free(value);

	return int_value;

error:
	if (f != NULL)
		fclose(f);
	free(field);
	free(value);
	return -1;
}

/* Terminates program if something unexpected happens. */
int get_process_exe(char *exe_name, const size_t bufsize, const pid_t pid)
{
	char path[PATH_MAX];
	ssize_t nbytes;
	int ret = snprintf(path, sizeof path, "/proc/%d/exe", (int) pid);
	if ((size_t) ret >= sizeof(path)) {
		log_error("weird: PATH_MAX exceeded (pid)\n");
		return -1;
	}
	nbytes = readlink(path, exe_name, bufsize);
	if (nbytes < 0 || ((size_t) nbytes) == bufsize) {
		log_error("Could not read link at %s: %s\n",
			  path, strerror(errno));
		exe_name[0] = 0;
		return -1;
	}
	exe_name[nbytes] = 0;
	return 0;
}

pid_t get_process_parent_pid(const pid_t pid)
{
	return read_process_attribute(pid, "PPid:", 0);
}

uid_t get_process_euid(const pid_t pid)
{
	return read_process_attribute(pid, "Uid:", 1);
}
