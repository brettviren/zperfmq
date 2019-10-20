#include "zperf_util.hpp"

// zsock_send() picture messages are really sprintf()'ed frames.
int pop_int(zmsg_t* msg)
{
    char *value = zmsg_popstr (msg);
    const int ret = atoi(value);
    free (value);
    return ret;
}

int64_t pop_long(zmsg_t *msg)
{
    char *value = zmsg_popstr (msg);
    const int64_t ret = atol(value);
    free (value);
    return ret;
}
