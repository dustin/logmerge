/*
 * Copyright (c) 2002-2007  Dustin Sallings
 */

#ifndef LOGFILES_H
#define LOGFILES_H 1

#include <iostream>
#include <queue>
#include <vector>

#include <sys/time.h>
#include <sys/types.h>
#include <zlib.h>

#define OK 1
#define ERROR -1

/* The size of a line buffer */
#define LINE_BUFFER 16384
/* Read buffer for gzipped data.  Fairly arbitrary, but just about anything
 * helps. */
#define GZBUFFER 1024*1024

extern "C" {

enum logType {
	COMMON, AMAZON_S3, UNKNOWN
};

/* The logfile itself */
struct logfile {
	/* The filename of this log entry */
	char *filename;
	/* The current record */
	char *line;
	/* Look!  I know pascal! */
	int lineLength;
	/* Function to output the current line */
	void (*outputLine)(struct logfile *);
	/* The timestamp of the current record */
	time_t timestamp;
	/* Indicate whether this logfile is open */
	bool isOpen;
	/* Buffering for speeding up gzipped file access */
	char *gzBufCur;
	char *gzBufEnd;
	char *gzBuf;
	/* The actual input stream being read */
	gzFile input;
};

class TimeCmp {
public:
	bool operator() (const struct logfile* a, const struct logfile* b) const;
};

typedef std::priority_queue<struct logfile *,
	std::vector<struct logfile *>, TimeCmp>
	log_queue;

/* Get a new logfile */
struct logfile *createLogfile(const char *filename);
/* Get the current record from the list */
struct logfile *currentRecord(const log_queue&);
/* Skip to the next record in the list */
void skipRecord(log_queue&);
/* Open a logfile */
int openLogfile(struct logfile *lf);

} // extern C

#endif /* LOGFILES_H */
