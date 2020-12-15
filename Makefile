CC ?= gcc
CFLAGS ?= -W -Wall -O2
LDFLAGS ?= 
EXES = sftpwrapper wtmplogger
COMMON_HEADERS = logger.h proc_pid_util.h config.h
COMMON_MODULES = config.c proc_pid_util.c

all:	$(EXES)

sftpwrapper: sftpwrapper.c $(COMMON_MODULES) $(COMMON_HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(COMMON_MODULES) $(LDFLAGS)

wtmplogger: wtmplogger.c $(COMMON_MODULES) $(COMMON_HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(COMMON_MODULES) $(LDFLAGS) -lutil

clean:
	rm -f $(EXES)

install:	$(EXES)
	install $(EXES) /usr/local/bin/
