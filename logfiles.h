/*
 * Copyright (c) 2002-2007  Dustin Sallings
 */

#ifndef LOGFILES_H
#define LOGFILES_H 1

#include <iostream>
#include <stdexcept>
#include <string>
#include <queue>
#include <vector>

#include <sys/time.h>
#include <sys/types.h>

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#ifdef USE_ASSERT
#define TESTED_STATIC
#else
#define TESTED_STATIC static
#endif

#define OK 1
#define ERROR -1

/* The size of a line buffer */
#define LINE_BUFFER 16384

enum logType {
    COMMON, AMAZON_S3, UNKNOWN
};

class LineOutputter {
public:
    virtual void writeLine(std::string s) = 0;
    virtual ~LineOutputter() {};
};

class DirectLineOutputter : public LineOutputter {
 public:
    void writeLine(std::string s);
};

class S3LineOutputter : public LineOutputter {
 public:
    void writeLine(std::string s);
};

/* The logfile itself */
class LogFile {
 public:

    LogFile(const char *);

    LogFile(const LogFile& lf) {
        throw std::runtime_error("Copying a logfile...");
    }

    ~LogFile();

    bool operator< (const LogFile &b) const {
        return timestamp > b.timestamp;
    }

    bool nextLine();
    void writeLine();

 private:

    void closeLogfile();
    time_t parseTimestamp();
    int openLogfile();

    LineOutputter *outputter;

    std::string *line;

    /* The filename of this log entry */
    char *filename;
    /* The timestamp of the current record */
    time_t timestamp;

    /* This is where my datas come from */
    std::ifstream *file;
    boost::iostreams::filtering_istream *instream;
};

typedef std::priority_queue<LogFile*, std::vector<LogFile*> > log_queue;

/* Skip to the next record in the list */
void skipRecord(log_queue&);

enum logType identifyLog(const char *line);

/* Parse a month.  This is generally static, but exposed when assertions are
   enabled. */
#ifdef USE_ASSERT
TESTED_STATIC int parseMonth(const char*);
#endif

#endif /* LOGFILES_H */
