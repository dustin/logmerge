# Copyright (c) 2002-2007  Dustin Sallings

SHELL=/bin/sh

CFLAGS=-O2 -Wall -Werror -g -DUSE_ASSERT
LDFLAGS=-O2 -g -lz

logmerge: logmerge.o
	$(CC) -o $@ logmerge.o $(LDFLAGS)

logmerge.o: logmerge.c logmerge.h

clean:
	rm -f *.o logmerge core
