/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 */

#include <iostream>
#include <string.h>
#include <unistd.h>

#ifdef USE_ASSERT
# include <assert.h>
#else
# undef assert
# define assert(a)
#endif

#include "logfiles.h"

#define STDOUT_BUF_SIZE 1024*1024

namespace logmerge {
    static void initLogfiles(log_queue&, int, char **);
    static void outputLogfiles(log_queue&);
    static void initLogfile(log_queue&, const char*);
    static void skipRecord(log_queue &queue);
}


/**
 * Get rid of the first entry in the log list, and reinsert it somewhere
 * that makes sense, or throw it away if it's no longer necessary.
 */
static void logmerge::skipRecord(log_queue &queue)
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

static void logmerge::initLogfile(log_queue& queue, const char *filename) {
    try {
        LogFile *lf=new LogFile(filename);
        queue.push(lf);
    } catch(LogfileError e) {
        std::cerr << "*** Error initializing ``" << filename << "'':  " << e.what()
                  << std::endl;
    }
}

/* Initialize all of the logfiles */
static void logmerge::initLogfiles(log_queue& queue, int argc, char **argv)
{
    if(argc>1) {
        for(int i=1; i<argc; i++) {
            initLogfile(queue, argv[i]);
        }
    } else {
        char buf[8192];
        if (isatty(STDIN_FILENO)) {
            std::cerr << "No logfiles given, accepting list from stdin"
                      << std::endl;
        }
        while(fgets((char*)&buf, sizeof(buf)-1, stdin)) {
            buf[strlen(buf)-1]=0x00;
            initLogfile(queue, buf);
        }
    }
}

static void logmerge::outputLogfiles(log_queue& queue)
{
    int entries=0;

    while(!queue.empty()) {
        entries++;

        LogFile *lf=queue.top();
        assert(lf);
        lf->writeLine();
        skipRecord(queue);
    }

    std::cerr << "Read " << entries << " entries" << std::endl;
}

#ifdef USE_ASSERT
static void testMonthParsing() {
    const char *months[] = {
        "Jan/", "Feb/", "Mar/", "Apr/", "May/", "Jun/",
        "Jul/", "Aug/", "Sep/", "Oct/", "Nov/", "Dec/"
    };
    for(int i=0; i<12; i++) {
        for(int j=0; j<10; j++) {
            int rv=parseMonth(months[i]);
            if(i != rv) {
                std::cerr << "Expected " << i << " for "
                          << months[i] << " got " << rv << std::endl;
                abort();
            }
        }
    }
    for(int j=0; j<10; j++) {
        for(int i=0; i<12; i++) {
            int rv=parseMonth(months[i]);
            if(i != rv) {
                std::cerr << "Expected " << i << " for "
                          << months[i] << " got " << rv << std::endl;
                abort();
            }
        }
    }
}
#else
static void testMonthParsing() {
}
#endif

/**
 * The main.
 */
int main(int argc, char **argv)
{
    log_queue queue;

    testMonthParsing();

    setvbuf(stdout, NULL, _IOFBF, STDOUT_BUF_SIZE);

    logmerge::initLogfiles(queue, argc, argv);
    logmerge::outputLogfiles(queue);

    return(0);
}
