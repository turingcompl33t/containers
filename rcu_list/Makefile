# hashmap/Makefile
#
# Makefile for chaining hashmap data structure.

CC = gcc
CFLAGS = -Wall -Werror -std=gnu11 -ggdb

CHECK_FLAGS = $(shell pkg-config --cflags --libs check)

OBJS = rcu_list.o

driver: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) check.c -o check $(CHECK_FLAGS)

check: driver
	./check

test: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) test.c -o test -pthread

clean:
	rm -f *~
	rm -f *.o
	rm -f check
	rm -f test 