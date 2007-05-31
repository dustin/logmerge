/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: logmerge.c,v 1.10 2005/03/28 19:10:39 dustin Exp $
 */

#include <stdio.h>
#include <string.h>

#ifdef USE_ASSERT
# include <assert.h>
#else
# define assert(a)
#endif

#ifdef USE_MYMALLOC
# include "mymalloc.h"
#endif

#include "logfiles.h"

#define NOTREACHED 0

static void dumpList(struct linked_list *list)
{
	struct linked_list *p;

	if(list==NULL) {
		fprintf(stderr, "*** dumpList: NULL list\n");
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
static struct linked_list *initLogfiles(int argc, char **argv)
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
		fprintf(stderr, "No logfiles given, accepting list from stdin\n");
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

static void outputLogfiles(struct linked_list *list)
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

	fprintf(stderr, "Read %d entries.\n", entries);
}

/**
 * The main.
 */
int main(int argc, char **argv)
{
	struct linked_list *list=NULL;

	list=initLogfiles(argc, argv);
	outputLogfiles(list);

#ifdef MYMALLOC
	_mdebug_dump();
#endif /* MYMALLOC */

	return(0);
}
