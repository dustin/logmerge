#include <iostream>
#include <stdexcept>
#include <string>

#include <stdio.h>
#include <time.h>
#include <sysexits.h>

#ifdef USE_ASSERT
# include <assert.h>
#else
# undef assert
# define assert(a)
#endif

#include <boost/regex.hpp>

#include "logfiles.h"

#define AMAZON_S3_REGEX "^[0-9a-f]+ ([-A-z0-9_\\.]+) \\[(.*)\\] ([0-9\\.]+) " \
    "[-0-9a-f]+ [0-9A-F]+ \\S+ \\S+ (\"[^\"]*\") (\\d+) [-A-z0-9]+ ([-0-9]+) " \
    "[-0-9]+ \\d+ [-0-9]+ (\"[^\"]*\") (\"[^\"]*\")"

boost::regex amazon_s3_regex(AMAZON_S3_REGEX, boost::regex::perl);

void S3LineOutputter::writeLine(const std::string &line) {
    boost::cmatch what;

    assert(!line.empty());

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

    if(boost::regex_search(line.c_str(), what, amazon_s3_regex)) {
        std::ostream_iterator<char> out(std::cout);
        what.format(out, "$3 - - [$2] $4 $5 $6 $7 $8 $1\n");
    } else {
        std::cerr << "*** S3: Failed to match ``" << line << "''" << std::endl;
    }
}

void DirectLineOutputter::writeLine(const std::string &line) {
    assert(!line.empty());
    std::cout << line << std::endl;
}

/* Returns a value from logTypes */
LineOutputter *LineOutputter::forLine(const std::string &line) {
    return (boost::regex_search(line.begin(), line.end(),
                                amazon_s3_regex)
            ? static_cast<LineOutputter*>(new S3LineOutputter)
            : static_cast<LineOutputter*>(new DirectLineOutputter));
}
