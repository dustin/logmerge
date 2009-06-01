/*
 * Copyright (c) 2002-2007  Dustin Sallings <dustin@spy.net>
 */

#include <iostream>

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

#include <boost/regex.hpp>

#include <zlib.h>

#include "logfiles.h"

#define NOTREACHED 0

#define AMAZON_S3_REGEX "^[0-9a-f]+ ([-A-z0-9_\\.]+) \\[(.*)\\] ([0-9\\.]+) " \
    "[0-9a-f]+ [0-9A-F]+ \\S+ \\S+ (\"[^\"]*\") (\\d+) [-A-z0-9]+ ([-0-9]+) " \
    "[-0-9]+ \\d+ [-0-9]+ (\"[^\"]*\") (\"[^\"]*\")"

boost::regex amazon_s3_regex(AMAZON_S3_REGEX, boost::regex::perl);

bool LogFile::myGzgets()
{
    char *rv=line;
    int s=LINE_BUFFER;
    int bytesRead=0;

    lineLength=0;

    for(;;) {
        if(gzBufCur > gzBufEnd || gzBufEnd == NULL) {
            /* Fetch some more stuff */
            bytesRead=gzread(input, gzBuf, GZBUFFER);
            gzBufEnd=bytesRead + gzBuf - 1;
            /* Make sure we got something */
            if(bytesRead == 0) {
                return(false);
            }
            gzBufCur=gzBuf;
        }
        /* Make sure we do not get too many characters */
        if(--s > 0) {
            *rv++ = *gzBufCur;
            lineLength++;
            if(*(gzBufCur++) == '\n') {
                *rv=0x00;
                return(true);
            }
        } else {
            *rv=0x00;
            return(true);
        }
    }

    assert(NOTREACHED);
    return(rv);
}

/* Returns a value from logTypes */
static enum logType identifyLog(const char *line) {
    enum logType rv=UNKNOWN;
    assert(line != NULL);

    if(boost::regex_search(line, amazon_s3_regex)) {
        rv=AMAZON_S3;
    } else {
        rv=COMMON;
    }
    return rv;
}

void S3LineOutputter::writeLine(const char *line, size_t length) {
    boost::cmatch what;

    assert(line);

    /*
    // Positions as defined in the regex
    S3_BUCKET   1
    S3_DATE     2
    S3_IP       3
    S3_REQ      4
    S3_STATUS   5
    S3_SIZE     6
    S3_REFER    7
    S3_UA       8
    */

    if(boost::regex_search(line, what, amazon_s3_regex)) {
        std::ostream_iterator<char> out(std::cout);
        what.format(out, "$3 - - [$2] $4 $5 $6 $7 $8 $1\n");
    } else {
        fprintf(stderr, "*** S3: Failed to match ``%s''\n", line);
    }
}

void DirectLineOutputter::writeLine(const char *line, size_t length) {
    assert(line != NULL);
    size_t bytes_written = fwrite(line, 1, length, stdout);
    if(bytes_written < length) {
        fprintf(stderr, "Short write: only wrote %d bytes out of %d\n",
                (unsigned int)bytes_written, (unsigned int)length);
        perror("fwrite");
        exit(EX_IOERR);
    }
}

/**
 * Open a logfile.
 */
int LogFile::openLogfile()
{
    int rv=ERROR;

    assert(filename);
    assert(! isOpen);

    fprintf(stderr, "*** Opening ``%s''\n", filename);

    input=gzopen(filename, "r");

    if(input != NULL) {
        isOpen=true;
        rv=OK;
    }

    /* Allocate the line buffer */
    line=(char*)calloc(1, LINE_BUFFER);
    assert(line != NULL);
    lineLength=0;

    /* Allocate the read buffer */
    gzBuf=(char*)calloc(1, GZBUFFER);
    assert(gzBuf != NULL);

    gzBufCur=NULL;
    gzBufEnd=NULL;

    return(rv);
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
    char *p;

    assert(line != NULL);

    timestamp=-1;

    p=line;

    try {

        /* The shortest line I can parse is about 32 characters. */
        if(lineLength < 32) {
            /* This is a broken entry */
            fprintf(stderr, "Broken log entry (too short):  %s\n", p);
        } else if(index(p, '[') != NULL) {
            struct tm tm;
            memset(&tm, 0x00, sizeof(tm));

            p=index(p, '[');
            /* Input validation */
            if(p == NULL || lineLength < 32) {
                fprintf(stderr, "invalid log line:  %s\n", line);
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
                fprintf(stderr,
                        "log line is starting to not look like CLF: %s\n",
                        line);
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
        fprintf(stderr, "* Error parsing timestamp from %s", line);
    }

    return(timestamp);
}

/**
 * Get the next line from a log file.
 * Return whether the seek actually occurred.
 */
bool LogFile::nextLine()
{
    bool rv=false;

    if(!isOpen) {
        int logfileOpened=openLogfile();
        /* This looks a little awkward, but it's the only way I can both
         * avoid the side effect of having assert perform the task and
         * not leave the variable unreferenced when assertions are off.
         */
        if(logfileOpened != OK) {
            assert(logfileOpened == OK);
        }
        /* Recurse to skip a line */
        rv=nextLine();
        assert(rv);
    }

    if(myGzgets()) {
        rv=true;
        char *p=line;
        /* Make sure the line is short enough */
        assert(lineLength < LINE_BUFFER);
        /* Make sure we read a line */
        if(p[lineLength-1] != '\n') {
            fprintf(stderr, "*** BROKEN LOG ENTRY IN %s (no newline)\n",
                    filename);
            rv=false;
        } else if(parseTimestamp() == -1) {
            /* If we can't parse the timestamp, give up */
            rv=false;
        }
    }

    return rv;
}

void LogFile::writeLine()
{
    if (! isOpen) {
        openLogfile();
    }

    outputter->writeLine(line, lineLength);
}

void LogFile::closeLogfile()
{
    int gzerrno=0;

    assert(input != NULL);
    assert(filename != NULL);

    fprintf(stderr, "*** Closing %s\n", filename);

    /* Free the line buffer */
    if(line != NULL) {
        free(line);
        line=NULL;
    }

    gzerrno=gzclose(input);
    if(gzerrno!=0) {
        gzerror(input, &gzerrno);
    }
    isOpen=false;

    if(gzBuf != NULL) {
        free(gzBuf);
        gzBuf = NULL;
    }

    gzBufCur=NULL;
    gzBufEnd=NULL;
}

/**
 * Get rid of a logfile that's no longer needed.
 */
LogFile::~LogFile()
{
    fprintf(stderr, "** Destroying %s\n", filename);

    if(isOpen) {
        closeLogfile();
    }

    delete outputter;

    /* Free the parts */
    if(filename!=NULL) {
        free(filename);
    }
    if(line != NULL) {
        free(line);
    }
    if(gzBuf != NULL) {
        free(gzBuf);
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

    isOpen = false;

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
            switch(identifyLog(line)) {
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
