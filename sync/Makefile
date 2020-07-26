# Makefile
#
# Makefile for rwlock.

CC = gcc
CFLAGS = -Wall -Werror -std=gnu11 -ggdb

LIB = sync.a
OBJS = event.o rwlock.o

$(LIB): $(OBJS)
	ar cr $@ $^

event.o: event.c event.h

rwlock.o: rwlock.c rwlock.h

clean:
	rm -f *~
	rm -f *.o
	rm -f $(LIB)