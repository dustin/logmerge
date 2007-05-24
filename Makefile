# Copyright (c) 2002-2007  Dustin Sallings

SHELL=/bin/sh

CFLAGS=-O2 -Wall -Werror -g -DUSE_ASSERT -DMYMALLOC
LDFLAGS=-O2 -g -lz

MYMALLOC_O=mymalloc.o

OBJS=logmerge.o $(MYMALLOC_O)

logmerge: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

logmerge.o: logmerge.c logmerge.h
mymalloc.o: mymalloc.c mymalloc.h

clean:
	rm -f $(OBJS) logmerge core
