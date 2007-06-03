/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: logmerge.c,v 1.10 2005/03/28 19:10:39 dustin Exp $
 */

#include <iostream>

#include <stdio.h>
#include <string.h>

#ifdef USE_ASSERT
# include <assert.h>
#else
# undef assert
# define assert(a)
#endif

#ifdef USE_MYMALLOC
# include "mymalloc.h"
#endif

extern "C" {
	#include "logfiles.h"
}

#define NOTREACHED 0

namespace logmerge {
	static void dumpList(struct linked_list *list);
	static struct linked_list *initLogfiles(int, char **);
	static void outputLogfiles(struct linked_list *);
}

static void logmerge::dumpList(struct linked_list *list)
{
	struct linked_list *p;

	if(list==NULL) {
		std::cerr << "*** dumpList: NULL list" << std::endl;
	} else {
		p=list;

		while(p!=NULL) {
			fprintf(stderr, "*** dumpList: %s (%d)\n", p->logfile->filename,
				(int)p->logfile->timestamp);
			p=p->next;
		}
	}
}

/* Initialize all of the logfiles and returned a linked list of them */
static struct linked_list *logmerge::initLogfiles(int argc, char **argv)
{
	struct linked_list *list=NULL;
	struct logfile *lf=NULL;
	int i=0;

	if(argc>1) {
		for(i=1; i<argc; i++) {
			lf=createLogfile(argv[i]);
			if(lf!=NULL) {
				list=addToList(list, lf);
			} else {
				fprintf(stderr, "Error opening logfile ``%s''\n", argv[i]);
			}
		}
	} else {
		char buf[8192];
		std::cerr << "No logfiles given, accepting list from stdin"
			<< std::endl;
		while(fgets((char*)&buf, sizeof(buf)-1, stdin)) {
			buf[strlen(buf)-1]=0x00;
			lf=createLogfile(buf);
			if(lf!=NULL) {
				list=addToList(list, lf);
			} else {
				fprintf(stderr, "Error opening logfile ``%s''\n", buf);
			}
		}
	}

	dumpList(list);
	return list;
}

static void logmerge::outputLogfiles(struct linked_list *list)
{
	int entries=0;
	struct logfile *lf=NULL;

	while(list!=NULL) {
		entries++;

		lf=currentRecord(list);
		assert(lf!=NULL);
		if(! lf->isOpen) {
			openLogfile(lf);
		}

#ifdef USE_MYMALLOC
		mymalloc_assert(lf);
		mymalloc_assert(lf->line);
		mymalloc_assert(lf->filename);
#endif
		lf->outputLine(lf);
		list=skipRecord(list);
	}

	std::cerr << "Read " << entries << " entries" << std::endl;
}

/**
 * The main.
 */
int main(int argc, char **argv)
{
	struct linked_list *list=NULL;

	list=logmerge::initLogfiles(argc, argv);
	logmerge::outputLogfiles(list);

#ifdef MYMALLOC
	_mdebug_dump();
#endif /* MYMALLOC */

	return(0);
}
