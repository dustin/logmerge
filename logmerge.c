/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: logmerge.c,v 1.10 2005/03/28 19:10:39 dustin Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif /* HAVE_ALLOCA_H */
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef USE_ASSERT
# include <assert.h>
#else
# define assert(a)
#endif

#include <zlib.h>

#include "logmerge.h"

#ifdef MYMALLOC
#include "mymalloc.h"
#endif

#define NOTREACHED 0

#define strdupa(d, s) { \
	int strdupaStrlen=strlen(s); \
	d=alloca(strdupaStrlen+1);\
	assert(d!=NULL); \
	memcpy(d, s, strdupaStrlen+1); \
}

static char *myGzgets(struct logfile *lf)
{
	char *rv=lf->line;
	int s=LINE_BUFFER;
	int bytesRead=0;

	for(;;) {
		if(lf->gzBufCur > lf->gzBufEnd || lf->gzBufEnd == NULL) {
			/* Fetch some more stuff */
			bytesRead=gzread(lf->input, lf->gzBuf, GZBUFFER);
			lf->gzBufEnd=bytesRead + lf->gzBuf - 1;
			/* Make sure we got something */
			if(bytesRead == 0) {
				return(NULL);
			}
			lf->gzBufCur=lf->gzBuf;
		}
		/* Make sure we do not get too many characters */
		if(--s > 0) {
			*rv++ = *lf->gzBufCur;
			if(*(lf->gzBufCur++) == '\n') {
				*rv=0x00;
				return(rv);
			}
		} else {
			*rv=0x00;
			return(rv);
		}
	}

	assert(NOTREACHED);
	return(rv);
}

/**
 * Open a logfile.
 */
static int openLogfile(struct logfile *lf)
{
	int rv=ERROR;
	assert(lf != NULL);

	assert(lf->isOpen==0);

	fprintf(stderr, "*** Opening %s\n", lf->filename);

	lf->input=gzopen(lf->filename, "r");

	if(lf->input != NULL) {
		lf->isOpen=1;
		rv=OK;
	}

	/* Allocate the line buffer */
	lf->line=calloc(1, LINE_BUFFER);
	assert(lf->line != NULL);

	/* Allocate the read buffer */
	lf->gzBuf=calloc(1, GZBUFFER);
	assert(lf->gzBuf != NULL);

	lf->gzBufCur=NULL;
	lf->gzBufEnd=NULL;

	return(rv);
}

/* Convert a three character month to the numeric value */
static int parseMonth(char *s)
{
	int rv=-1;

	assert(s != NULL);
	assert(s[0] != 0x00);
	assert(s[1] != 0x00);
	assert(s[2] != 0x00);

	if(strncmp(s, "Jan", 3)==0) {
		rv=0;
	} else if(strncmp(s, "Feb", 3)==0) {
		rv=1;
	} else if(strncmp(s, "Mar", 3)==0) {
		rv=2;
	} else if(strncmp(s, "Apr", 3)==0) {
		rv=3;
	} else if(strncmp(s, "May", 3)==0) {
		rv=4;
	} else if(strncmp(s, "Jun", 3)==0) {
		rv=5;
	} else if(strncmp(s, "Jul", 3)==0) {
		rv=6;
	} else if(strncmp(s, "Aug", 3)==0) {
		rv=7;
	} else if(strncmp(s, "Sep", 3)==0) {
		rv=8;
	} else if(strncmp(s, "Oct", 3)==0) {
		rv=9;
	} else if(strncmp(s, "Nov", 3)==0) {
		rv=10;
	} else if(strncmp(s, "Dec", 3)==0) {
		rv=11;
	}

	return(rv);
}

static time_t parseTimestamp(struct logfile *lf)
{
	char *p;

	assert(lf != NULL);
	assert(lf->line != NULL);

	lf->timestamp=-1;

	memset(&lf->tm, 0x00, sizeof(lf->tm));

	p=lf->line;

	/* The shortest line I can parse is about 32 characters. */
	if(strlen(p) < 32) {
		/* This is a broken entry */
		fprintf(stderr, "Broken log entry (too short):  %s\n", p);
	} else if(index(p, '[') != NULL) {

		p=index(p, '[');
		/* Input validation */
		if(p == NULL || strlen(p) < 32) {
			fprintf(stderr, "invalid log line:  %s\n", lf->line);
			goto catch;
		}

		/* fprintf(stderr, "**** Parsing %s\n", p); */
		p++;
		lf->tm.tm_mday=atoi(p);
		p+=3;
		lf->tm.tm_mon=parseMonth(p);
		p+=4;
		lf->tm.tm_year=atoi(p);
		p+=5;
		lf->tm.tm_hour=atoi(p);
		p+=3;
		lf->tm.tm_min=atoi(p);
		p+=3;
		lf->tm.tm_sec=atoi(p);

		/* Make sure it still looks like CLF */
		if(p[2] != ' ') {
			fprintf(stderr, "log line is starting to not look like CLF: %s\n",
				lf->line);
			goto catch;
		}

		lf->tm.tm_year-=1900;

		/* Let mktime guess the timezone */
		lf->tm.tm_isdst=-1;

		lf->timestamp=mktime(&lf->tm);

	} else {
		fprintf(stderr, "Unknown log format:  %s\n", p);
	}

	catch:

	if(lf->timestamp < 0) {
		fprintf(stderr, "* Error parsing timestamp from %s", lf->line);
	}

	return(lf->timestamp);
}

/**
 * Get the next line from a log file.
 */
static char *nextLine(struct logfile *lf)
{
	char *p=NULL;

	assert(lf != NULL);

	if(lf->isOpen == 0) {
		int logfileOpened=openLogfile(lf);
		/* This looks a little awkward, but it's the only way I can both
		 * avoid the side effect of having assert perform the task and
		 * not leave the variable unreferenced when assertions are off.
		 */
		if(logfileOpened != OK) {
			assert(logfileOpened == OK);
		}
		/* Recurse to skip a line */
		p=nextLine(lf);
		assert(p!=NULL);
	}

	p=myGzgets(lf);
	if(p!=Z_NULL) {
		/* At this point, p and lf->line represent the same location */
		/* Make sure the line is short enough */
		assert(strlen(p) < LINE_BUFFER);
		/* Make sure we read a line */
		if(p[strlen(p)-1] != '\n') {
			fprintf(stderr, "*** BROKEN LOG ENTRY IN %s (no newline)\n",
				lf->filename);
			p=NULL;
		} else if(parseTimestamp(lf) == -1) {
			/* If we can't parse the timestamp, give up */
			p=NULL;
		}
	}

	return(p);
}

static void closeLogfile(struct logfile *lf)
{
	int gzerrno=0;

	assert(lf != NULL);
	assert(lf->input != NULL);
	assert(lf->filename != NULL);

	fprintf(stderr, "*** Closing %s\n", lf->filename);

	/* Free the line buffer */
	if(lf->line != NULL) {
		free(lf->line);
		lf->line=NULL;
	}

	gzerrno=gzclose(lf->input);
	if(gzerrno!=0) {
		gzerror(lf->input, &gzerrno);
	}
	lf->isOpen=0;

	if(lf->gzBuf != NULL) {
		free(lf->gzBuf);
		lf->gzBuf = NULL;
	}

	lf->gzBufCur=NULL;
	lf->gzBufEnd=NULL;
}

/**
 * Get rid of a logfile that's no longer needed.
 */
static void destroyLogfile(struct logfile *lf)
{
	assert(lf != NULL);

	fprintf(stderr, "** Destroying %s\n", lf->filename);

	if(lf->isOpen==1) {
		closeLogfile(lf);
	}

	/* Free the parts */
	if(lf->filename!=NULL) {
		free(lf->filename);
	}
	if(lf->line != NULL) {
		free(lf->line);
	}
	if(lf->gzBuf != NULL) {
		free(lf->gzBuf);
	}

	/* Lastly, free the container itself. */
	free(lf);
}

/**
 * Create a new logfile.
 */
struct logfile *createLogfile(const char *filename)
{
	struct logfile *rv=NULL;
	char *p=NULL;

	rv=calloc(1, sizeof(struct logfile));
	assert(rv != NULL);

	rv->filename=(char *)strdup(filename);
	assert(rv->filename != NULL);

	/* Try to open the logfile */
	if(openLogfile(rv) != OK) {
		destroyLogfile(rv);
		rv=NULL;
	} else {
		/* If it's opened succesfully, read the next (first) line */
		p=nextLine(rv);
		/* If nextLine didn't return a record, this entry is invalid. */
		if(p == NULL) {
			destroyLogfile(rv);
			rv=NULL;
		} else {
			/* Otherwise, it's valid and we'll proceed, but close it. */
			closeLogfile(rv);
		}
	}

	return(rv);
}

/**
 * Add a logfile to the given linked list.  If the list is NULL, create a
 * new one.
 */
struct linked_list *addToList(struct linked_list *list, struct logfile *r)
{
	struct linked_list *tmp;
	struct linked_list *rv;

	assert(r != NULL);

	tmp=calloc(1, sizeof(struct linked_list));

	tmp->logfile=r;

	if(list == NULL) {
		rv=tmp;
	} else {

		assert(list->logfile != NULL);

		if(tmp->logfile->timestamp < list->logfile->timestamp) {
			/* Special case where it goes at the head. */
			rv=tmp;
			tmp->next=list;
		} else {
			/* Regular case where it goes somewhere in the list. */
			struct linked_list *p;
			int placed=0;
			
			rv=list;
			p=list;
			while(placed==0 && p->next != NULL) {

				if(tmp->logfile->timestamp < p->next->logfile->timestamp) {
					tmp->next=p->next;
					p->next=tmp;
					placed=1;
				}

				p=p->next;
			}

			/* If it's still not placed, it goes at the end */
			if(placed==0) {
				p->next=tmp;
			}
		}
	}


	return(rv);
}

/**
 * Get the current record from the first entry in the linked list.
 */
struct logfile *currentRecord(struct linked_list *list)
{
	struct logfile *rv=NULL;

	if(list!=NULL && list->logfile != NULL) {
		rv=list->logfile;
	}

	return(rv);
}

/**
 * Get rid of the first entry in the log list, and reinsert it somewhere
 * that makes sense, or throw it away if it's no longer necessary.
 */
struct linked_list *skipRecord(struct linked_list *list)
{
	struct linked_list *rv=NULL;
	struct logfile *oldEntry=NULL;
	char *p;

	if(list!=NULL) {
		rv=list->next;

		oldEntry=list->logfile;

		p=nextLine(oldEntry);
		/* If stuff comes back, reinsert the old entry */
		if(p!=NULL) {
			rv=addToList(rv, oldEntry);
		} else {
			destroyLogfile(oldEntry);
		}

		/* No longer need this record, a new one will be created when it's
		 * reinserted (if it's reinserted)
		 */
		free(list);
	}

	return(rv);
}

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

/**
 * The main.
 */
int main(int argc, char **argv)
{
	struct linked_list *list=NULL;
	struct logfile *lf=NULL;
	int i=0;
	int entries=0;

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
		if(lf->line) {
			printf("%s", lf->line);
		}
		list=skipRecord(list);
	}

	fprintf(stderr, "Read %d entries.\n", entries);

#ifdef MYMALLOC
	_mdebug_dump();
#endif /* MYMALLOC */

	return(0);
}
