/*
 * Copyright (c) 2002-2007  Dustin Sallings <dustin@spy.net>
 */

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sysexits.h>

#ifdef USE_ASSERT
# include <assert.h>
#else
# undef assert
# define assert(a)
#endif

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "logfiles.h"

#define NOTREACHED 0

namespace io = boost::iostreams;

/**
 * Open a logfile.
 */
int LogFile::openLogfile()
{
    assert(filename);
    assert(instream == NULL);

    fprintf(stderr, "*** Opening ``%s''\n", filename);

    file = new std::ifstream(filename,
                             std::ios_base::in | std::ios_base::binary);
    assert(file != NULL);
    if (file->fail()) {
        throw std::runtime_error("Error opening logfile.");
    }
    assert(file->is_open());

    bool isgzip = file->get() == 037 && file->get() == 0213;
    if (file->fail()) {
        throw std::runtime_error("Error trying to read magic");
    }
    file->seekg(0, std::ios_base::beg);

    instream = new io::filtering_istream();
    assert(instream != NULL);
    if (isgzip) {
        instream->push(io::gzip_decompressor());
    }
    instream->push(*file);

    assert(!instream->fail());

    return(OK);
}

/* A date and a string */
struct date_str {
    char *datestr;
    int val;
};

#define MONTH_JAN       (((((('J'<<8)|'a')<<8)|'n')<<8)|'/')
#define MONTH_FEB       (((((('F'<<8)|'e')<<8)|'b')<<8)|'/')
#define MONTH_MAR       (((((('M'<<8)|'a')<<8)|'r')<<8)|'/')
#define MONTH_APR       (((((('A'<<8)|'p')<<8)|'r')<<8)|'/')
#define MONTH_MAY       (((((('M'<<8)|'a')<<8)|'y')<<8)|'/')
#define MONTH_JUN       (((((('J'<<8)|'u')<<8)|'n')<<8)|'/')
#define MONTH_JUL       (((((('J'<<8)|'u')<<8)|'l')<<8)|'/')
#define MONTH_AUG       (((((('A'<<8)|'u')<<8)|'g')<<8)|'/')
#define MONTH_SEP       (((((('S'<<8)|'e')<<8)|'p')<<8)|'/')
#define MONTH_OCT       (((((('O'<<8)|'c')<<8)|'t')<<8)|'/')
#define MONTH_NOV       (((((('N'<<8)|'o')<<8)|'v')<<8)|'/')
#define MONTH_DEC       (((((('D'<<8)|'e')<<8)|'c')<<8)|'/')

/* Convert a three character month to the numeric value */
TESTED_STATIC int parseMonth(const char *input) {
    int rv=-1;
    int inputInt=0;

    for(int i=0; i<4 && input[i]; i++) {
        inputInt = (inputInt << 8) | input[i];
    }

    switch(inputInt) {
    case MONTH_JAN: rv=0; break;
    case MONTH_FEB: rv=1; break;
    case MONTH_MAR: rv=2; break;
    case MONTH_APR: rv=3; break;
    case MONTH_MAY: rv=4; break;
    case MONTH_JUN: rv=5; break;
    case MONTH_JUL: rv=6; break;
    case MONTH_AUG: rv=7; break;
    case MONTH_SEP: rv=8; break;
    case MONTH_OCT: rv=9; break;
    case MONTH_NOV: rv=10; break;
    case MONTH_DEC: rv=11; break;
    }

    return rv;
}

class BadTimestamp : public std::exception {
    virtual const char* what() const throw() {
        return "Timestamp parse error";
    }
};

time_t LogFile::parseTimestamp()
{
    const char *p;

    assert(line);
    assert(!line->empty());

    timestamp=-1;

    p=line->c_str();

    try {

        /* The shortest line I can parse is about 32 characters. */
        if(line->length() < 32) {
            /* This is a broken entry */
            fprintf(stderr, "Broken log entry (too short):  %s\n", p);
        } else if(index(p, '[') != NULL) {
            struct tm tm;
            memset(&tm, 0x00, sizeof(tm));

            p=index(p, '[');
            /* Input validation */
            if(p == NULL || line->length() < 32) {
                std::cerr << "Invalid log line:  " << *line << std::endl;
                throw BadTimestamp();
            }

            /* fprintf(stderr, "**** Parsing %s\n", p); */
            p++;
            tm.tm_mday=atoi(p);
            p+=3;
            tm.tm_mon=parseMonth(p);
            p+=4;
            tm.tm_year=atoi(p);
            p+=5;
            tm.tm_hour=atoi(p);
            p+=3;
            tm.tm_min=atoi(p);
            p+=3;
            tm.tm_sec=atoi(p);

            /* Make sure it still looks like CLF */
            if(p[2] != ' ') {
                std::cerr << "log line is starting to not look like CLF: "
                          << *line
                          << std::endl;
                throw BadTimestamp();
            }

            tm.tm_year-=1900;

            /* Let mktime guess the timezone */
            tm.tm_isdst=-1;

            timestamp=mktime(&tm);

        } else {
            fprintf(stderr, "Unknown log format:  %s\n", p);
        }

    } catch(BadTimestamp e) {
        // Damn.
    }

    if(timestamp < 0) {
        std::cerr << "* Error parsing timestamp from " << *line << std::endl;
    }

    return(timestamp);
}

/**
 * Get the next line from a log file.
 * Return whether the seek actually occurred.
 */
bool LogFile::nextLine()
{
    bool rv=true;

    if(instream == NULL) {
        int logfileOpened=openLogfile();
        /* This looks a little awkward, but it's the only way I can both
         * avoid the side effect of having assert perform the task and
         * not leave the variable unreferenced when assertions are off.
         */
        if(logfileOpened != OK) {
            assert(logfileOpened == OK);
        }
        /* Recurse to skip a line */
        std::cerr << "Recursing..." << std::endl;
        rv=nextLine();
        assert(rv);
    }

    if (line == NULL) {
        line = new std::string();
    }

    std::getline(*instream, *line);
    rv = !(instream->fail() || line->empty() || parseTimestamp() == -1);

    return rv;
}

void LogFile::writeLine()
{
    if (instream == NULL) {
        openLogfile();
        nextLine();
    }

    assert(line);
    outputter->writeLine(*line);
}

void LogFile::closeLogfile()
{
    assert(instream != NULL);
    assert(filename != NULL);

    fprintf(stderr, "*** Closing %s\n", filename);

    delete instream;
    instream = NULL;
    delete file;
    file = NULL;

    if (line) {
        delete line;
        line = NULL;
    }
}

/**
 * Get rid of a logfile that's no longer needed.
 */
LogFile::~LogFile()
{
    fprintf(stderr, "** Destroying %s\n", filename);

    if(instream != NULL) {
        closeLogfile();
    }

    delete outputter;

    /* Free the parts */
    if(filename!=NULL) {
        free(filename);
    }
}

/**
 * Create a new logfile.
 */
LogFile::LogFile(const char *inFilename)
{
    assert(inFilename);
    filename=(char *)strdup(inFilename);
    assert(filename);

    instream = NULL;
    line = NULL;
    outputter = NULL;
    file = NULL;

    /* Try to open the logfile */
    if(openLogfile() != OK) {
        throw std::runtime_error("Error opening logfile.");
    } else {
        /* If it's opened succesfully, read the next (first) line */
        if(!nextLine()) {
            /* If nextLine didn't return a record, this entry is invalid. */
            throw std::runtime_error("Error trying to read a record.");
        } else {
            /* Otherwise, it's valid and we'll proceed, but close it. */
            switch(identifyLog(line->c_str())) {
            case COMMON:
                fprintf(stderr, "**** %s is a common log file\n", filename);
                outputter = new DirectLineOutputter();
                break;
            case AMAZON_S3:
                fprintf(stderr, "**** %s is an s3 log file\n", filename);
                outputter = new S3LineOutputter();
                break;
            case UNKNOWN:
                fprintf(stderr, "! Can't identify type of %s\n", filename);
                throw std::runtime_error("Found no output line.");
                break;
            default:
                throw std::runtime_error("Found no output line.");
                assert(false);
            }

            closeLogfile();
        }
    }
}

/**
 * Get rid of the first entry in the log list, and reinsert it somewhere
 * that makes sense, or throw it away if it's no longer necessary.
 */
void skipRecord(log_queue& queue)
{
    assert(!queue.empty());

    LogFile *oldEntry=queue.top();
    assert(oldEntry);
    queue.pop();

    /* If stuff comes back, reinsert the old entry */
    if(oldEntry->nextLine()) {
        queue.push(oldEntry);
    } else {
        delete oldEntry;
    }
}
