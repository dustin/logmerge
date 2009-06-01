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

#ifdef USE_ASSERT
#define TESTED_STATIC
#else
#define TESTED_STATIC static
#endif

#define OK 1
#define ERROR -1

/* The size of a line buffer */
#define LINE_BUFFER 16384
/* Read buffer for gzipped data.  Fairly arbitrary, but just about anything
 * helps. */
#define GZBUFFER 1024*1024

enum logType {
    COMMON, AMAZON_S3, UNKNOWN
};

class LineOutputter {
public:
    virtual void writeLine(const char *line, size_t length) = 0;
    virtual ~LineOutputter() {};
};

class DirectLineOutputter : public LineOutputter {
 public:
    void writeLine(const char *line, size_t length);
};

class S3LineOutputter : public LineOutputter {
 public:
    void writeLine(const char *line, size_t length);
};

/* The logfile itself */
class LogFile {
 public:

    LogFile(const char *);

    LogFile(const LogFile& lf) {
        throw "Copying a logfile...";
    }

    ~LogFile();

    bool operator< (const LogFile &b) const {
        return timestamp > b.timestamp;
    }

    bool nextLine();

    /* The current record */
    char *line;
    /* Look!  I know pascal! */
    size_t lineLength;
    /* Indicate whether this logfile is open */
    bool isOpen;
    int openLogfile();
    LineOutputter *outputter;

 private:

    void closeLogfile();
    bool myGzgets();
    time_t parseTimestamp();

    /* The filename of this log entry */
    char *filename;
    /* The timestamp of the current record */
    time_t timestamp;
    /* Buffering for speeding up gzipped file access */
    char *gzBufCur;
    char *gzBufEnd;
    char *gzBuf;
    /* The actual input stream being read */
    gzFile input;
};

typedef std::priority_queue<LogFile*, std::vector<LogFile*> > log_queue;

/* Skip to the next record in the list */
void skipRecord(log_queue&);

/* Parse a month.  This is generally static, but exposed when assertions are
   enabled. */
#ifdef USE_ASSERT
TESTED_STATIC int parseMonth(const char*);
#endif

#endif /* LOGFILES_H */
