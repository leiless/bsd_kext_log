#
# Makefile  Created 190417
#

CC?=gcc
CFLAGS+=-std=c99 -xc -Wall -Wextra -Werror

all: debug

kextlog_daemon.o: kextlog_daemon.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -c

kextlog_daemon: kextlog_daemon.o
	$(CC) -o $@ $^

release: CFLAGS += -O2
release: kextlog_daemon

kextlog_daemon-debug: kextlog_daemon.o
	$(CC) -o $@ $^

debug: CPPFLAGS += -DDEBUG
debug: CFLAGS += -g -O0
debug: kextlog_daemon-debug

clean:
	rm -f *.o kextlog_daemon kextlog_daemon-debug

