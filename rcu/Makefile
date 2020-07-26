# rcu/Makefile
#
# Makefile for RCU system.

MAKE = make

S = ../sync

CC = gcc
CFLAGS = -Wall -Werror -std=gnu11 -ggdb

CHECK_FLAGS = $(shell pkg-config --cflags --libs check)

LIB = rcu.a
OBJS = rcu.o gc.o priority_queue.o intrusive_list.o $S/event.o

$(LIB): $(OBJS)
	ar cr $@ $^

rcu.o: rcu.c rcu.h
gc.o: gc.c gc.h
priority_queue.o: priority_queue.c priority_queue.h
intrusive_list.o: intrusive_list.c intrusive_list.h

driver: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) test/driver.c -o driver -pthread

clean:
	rm -f *~
	rm -f *.o
	rm -f $(LIB)
	rm -f driver
	cd ../sync; $(MAKE) clean