#ifndef ZPERF_UTIL_H
#define ZPERF_UTIL_H

#include <czmq.h>


int pop_int(zmsg_t* msg);
int64_t pop_long(zmsg_t *msg);


#endif
