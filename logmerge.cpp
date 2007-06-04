/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: logmerge.c,v 1.10 2005/03/28 19:10:39 dustin Exp $
 */

#include <iostream>

#ifdef USE_ASSERT
# include <assert.h>
#else
# undef assert
# define assert(a)
#endif

#include "logfiles.h"

namespace logmerge {
	static void initLogfiles(log_queue&, int, char **);
	static void outputLogfiles(log_queue);
}

/* Initialize all of the logfiles */
static void logmerge::initLogfiles(log_queue& queue, int argc, char **argv)
{
	struct logfile *lf=NULL;
	int i=0;

	if(argc>1) {
		for(i=1; i<argc; i++) {
			lf=createLogfile(argv[i]);
			if(lf!=NULL) {
				queue.push(lf);
			} else {
				std::cerr << "Error opening logfile ``" << argv[i] << "''"
					<< std::endl;
			}
		}
	} else {
		char buf[8192];
		std::cerr << "No logfiles given, accepting list from stdin"
			<< std::endl;
		while(fgets((char*)&buf, sizeof(buf)-1, stdin)) {
			buf[strlen(buf)-1]=0x00;
			lf=createLogfile(buf);
			if(lf!=NULL) {
				queue.push(lf);
			} else {
				std::cerr << "Error opening logfile ``" << buf << "''"
					<< std::endl;
			}
		}
	}
}

static void logmerge::outputLogfiles(log_queue queue)
{
	int entries=0;
	struct logfile *lf=NULL;

	while(!queue.empty()) {
		entries++;

		lf=currentRecord(queue);
		assert(lf!=NULL);
		if(! lf->isOpen) {
			openLogfile(lf);
		}

		lf->outputLine(lf);
		skipRecord(queue);
	}

	std::cerr << "Read " << entries << " entries" << std::endl;
}

/**
 * The main.
 */
int main(int argc, char **argv)
{
	log_queue queue;

	logmerge::initLogfiles(queue, argc, argv);
	logmerge::outputLogfiles(queue);

	return(0);
}
