/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 */

#include <iostream>
#include <string.h>

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
}

static void logmerge::initLogfile(log_queue& queue, const char *filename) {
    struct logfile *lf=createLogfile(filename);
    if(lf!=NULL) {
        queue.push(lf);
    } else {
        std::cerr << "Error opening logfile ``" << filename
                  << "''" << std::endl;
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
        std::cerr << "No logfiles given, accepting list from stdin"
                  << std::endl;
        while(fgets((char*)&buf, sizeof(buf)-1, stdin)) {
            buf[strlen(buf)-1]=0x00;
            initLogfile(queue, buf);
        }
    }
}

static void logmerge::outputLogfiles(log_queue& queue)
{
    int entries=0;
    struct logfile *lf=NULL;

    while(!queue.empty()) {
        entries++;

        lf=queue.top();
        assert(lf!=NULL);
        if(! lf->isOpen) {
            openLogfile(lf);
        }

        lf->outputLine(lf);
        skipRecord(queue);
    }

    std::cerr << "Read " << entries << " entries" << std::endl;
}

#ifdef USE_ASSERT
static void testMonthParsing() {
    char *months[] = {
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
