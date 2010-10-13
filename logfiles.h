/* -*- mode: c++ -*- */
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

class LineOutputter {
public:
    virtual void writeLine(const std::string &s) = 0;
    virtual ~LineOutputter() {};

    static LineOutputter *forLine(const std::string &s);
};

class DirectLineOutputter : public LineOutputter {
 public:
    void writeLine(const std::string &s);
};

class S3LineOutputter : public LineOutputter {
 public:
    void writeLine(const std::string &s);
};

class LogfileError : public std::runtime_error {
public:
    LogfileError(const char *s) : std::runtime_error(s) { };
};

/* The logfile itself */
class LogFile {
 public:

    LogFile(const char *);

    ~LogFile();

    bool nextLine();
    void writeLine();

    const time_t getTimestamp() const { return timestamp; }

 private:

    LogFile(const LogFile& lf);

    void closeLogfile();
    time_t parseTimestamp();
    void openLogfile();

    LineOutputter *outputter;

    std::string line;

    /* The filename of this log entry */
    std::string filename;
    /* The timestamp of the current record */
    time_t timestamp;

    /* This is where my datas come from */
    std::ifstream *file;
    boost::iostreams::filtering_istream *instream;
};

struct log_compare {
    bool operator() (const LogFile *lf1, const LogFile *lf2) {
        return lf1->getTimestamp() > lf2->getTimestamp();
    }
};

typedef std::priority_queue<LogFile*, std::vector<LogFile*>, log_compare> log_queue;

/* Parse a month.  This is generally static, but exposed when assertions are
   enabled. */
#ifdef USE_ASSERT
TESTED_STATIC int parseMonth(const char*);
#endif

#endif /* LOGFILES_H */
