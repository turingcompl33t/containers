# flat-map/Makefile
#
# Makefile for flat hashmap data structure.

CC = gcc
CFLAGS = -Wall -Werror -std=gnu11 -ggdb

CHECK_FLAGS = $(shell pkg-config --cflags --libs check)

LIB = flat_map

lib: $(LIB).o murmur3.o

$(LIB).o: $(LIB).c $(LIB).h
murmur3.o: murmur3.c murmur3.h

driver: lib
	$(CC) $(CFLAGS) check.c $(LIB).o murmur3.o -o check $(CHECK_FLAGS)

check: driver
	./check

clean:
	rm -f *~
	rm -f *.o
	rm -f $(LIB).o
	rm -f check