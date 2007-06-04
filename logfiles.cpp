/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: logmerge.c,v 1.10 2005/03/28 19:10:39 dustin Exp $
 */

#include <iostream>

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef USE_ASSERT
# include <assert.h>
#else
# undef assert
# define assert(a)
#endif

#include <boost/regex.hpp>

#include <zlib.h>

#include "logfiles.h"

#define NOTREACHED 0

#define AMAZON_S3_REGEX "^[0-9a-f]+ ([-A-z0-9_\\.]+) \\[(.*)\\] ([0-9\\.]+) " \
	"[0-9a-f]+ [0-9A-F]+ \\S+ \\S+ (\"[^\"]*\") (\\d+) [-A-z0-9]+ ([-0-9]+) " \
	"[-0-9]+ \\d+ [-0-9]+ (\"[^\"]*\") (\"[^\"]*\")"

boost::regex amazon_s3_regex(AMAZON_S3_REGEX, boost::regex::perl);

bool TimeCmp::operator() (const struct logfile* a, const struct logfile* b)
	const
{
	return a->timestamp > b->timestamp;
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

/* Returns a value from logTypes */
static int identifyLog(const char *line) {
	int rv=UNKNOWN;
	assert(line != NULL);

	if(boost::regex_search(line, amazon_s3_regex)) {
		rv=AMAZON_S3;
	} else {
		rv=COMMON;
	}
	return rv;
}

static void outputLineS3(struct logfile *lf) {
	boost::cmatch what;

	assert(lf);
	assert(lf->line);

/*
// Positions as defined in the regex
S3_BUCKET	1
S3_DATE		2
S3_IP		3
S3_REQ		4
S3_STATUS	5
S3_SIZE		6
S3_REFER	7
S3_UA		8
*/

	if(boost::regex_search(lf->line, what, amazon_s3_regex)) {
		std::ostream_iterator<char> out(std::cout);
		what.format(out, "$3 - - [$2] $4 $5 $6 $7 $8 $1\n");
	} else {
		fprintf(stderr, "*** S3: Failed to match ``%s''\n", lf->line);
	}
}

static void outputLineDirect(struct logfile *lf) {
	assert(lf != NULL);
	assert(lf->line != NULL);
	printf("%s", lf->line);
}

/**
 * Open a logfile.
 */
int openLogfile(struct logfile *lf)
{
	int rv=ERROR;
	assert(lf != NULL);

	assert(! lf->isOpen);

	fprintf(stderr, "*** Opening %s\n", lf->filename);

	lf->input=gzopen(lf->filename, "r");

	if(lf->input != NULL) {
		lf->isOpen=true;
		rv=OK;
	}

	/* Allocate the line buffer */
	lf->line=(char*)calloc(1, LINE_BUFFER);
	assert(lf->line != NULL);

	/* Allocate the read buffer */
	lf->gzBuf=(char*)calloc(1, GZBUFFER);
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

class BadTimestamp : public std::exception {
	virtual const char* what() const throw() {
		return "Timestamp parse error";
	}
};

static time_t parseTimestamp(struct logfile *lf)
{
	char *p;

	assert(lf != NULL);
	assert(lf->line != NULL);

	lf->timestamp=-1;

	memset(&lf->tm, 0x00, sizeof(lf->tm));

	p=lf->line;

	try {

		/* The shortest line I can parse is about 32 characters. */
		if(strlen(p) < 32) {
			/* This is a broken entry */
			fprintf(stderr, "Broken log entry (too short):  %s\n", p);
		} else if(index(p, '[') != NULL) {

			p=index(p, '[');
			/* Input validation */
			if(p == NULL || strlen(p) < 32) {
				fprintf(stderr, "invalid log line:  %s\n", lf->line);
				throw BadTimestamp();
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
				fprintf(stderr,
					"log line is starting to not look like CLF: %s\n",
					lf->line);
				throw BadTimestamp();
			}

			lf->tm.tm_year-=1900;

			/* Let mktime guess the timezone */
			lf->tm.tm_isdst=-1;

			lf->timestamp=mktime(&lf->tm);

		} else {
			fprintf(stderr, "Unknown log format:  %s\n", p);
		}

	} catch(BadTimestamp e) {
		// Damn.
	}

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

	if(!lf->isOpen) {
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

	return p == NULL ? NULL : lf->line;
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
	lf->isOpen=false;

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

	if(lf->isOpen) {
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

	rv=(struct logfile *)calloc(1, sizeof(struct logfile));
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
			switch(identifyLog(p)) {
				case COMMON:
					fprintf(stderr, "**** %s is a common log file\n", filename);
					rv->outputLine=outputLineDirect;
					break;
				case AMAZON_S3:
					fprintf(stderr, "**** %s is an s3 log file\n", filename);
					rv->outputLine=outputLineS3;
					break;
				case UNKNOWN:
					fprintf(stderr, "! Can't identify type of %s\n", filename);
					break;
				default:
					assert(0);
			}

			if(rv->outputLine == NULL) {
				destroyLogfile(rv);
				rv=NULL;
			} else {
				closeLogfile(rv);
			}
		}
	}

	return(rv);
}

/**
 * Get the current record from the first entry in the linked list.
 */
struct logfile *currentRecord(const log_queue& queue)
{
	struct logfile *rv=NULL;

	if(!queue.empty()) {
		rv=queue.top();
	}

	return(rv);
}

/**
 * Get rid of the first entry in the log list, and reinsert it somewhere
 * that makes sense, or throw it away if it's no longer necessary.
 */
void skipRecord(log_queue& queue)
{
	struct logfile *oldEntry=NULL;
	char *p;

	if(!queue.empty()) {
		oldEntry=queue.top();
		queue.pop();

		p=nextLine(oldEntry);
		/* If stuff comes back, reinsert the old entry */
		if(p!=NULL) {
			queue.push(oldEntry);
		} else {
			destroyLogfile(oldEntry);
		}
	}
}
