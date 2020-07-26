# hashmap/Makefile
#
# Makefile for chaining hashmap data structure.

CC = gcc
CFLAGS = -Wall -Werror -std=gnu11 -ggdb

CHECK_FLAGS = $(shell pkg-config --cflags --libs check)

OBJS = hashmap.o hashmap_attr.o intrusive_list.o murmur3.o

hashmap.o: hashmap.c hashmap.h
hashmap_attr.o: hashmap_attr.c hashmap_attr.h
intrusive_list.o: intrusive_list.c intrusive_list.h
murmur3.o: murmur3.c murmur3.h

driver: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) check.c -o check $(CHECK_FLAGS)

check: driver
	./check

clean:
	rm -f *~
	rm -f *.o
	rm -f check