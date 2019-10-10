/*  =========================================================================
    zperf_msg - class description

    LGPL 3.0
    =========================================================================
*/

#ifndef ZPERF_MSG_H_INCLUDED
#define ZPERF_MSG_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Create a new zperf_msg
ZPERFMQ_EXPORT zperf_msg_t *
    zperf_msg_new (void);

//  Destroy the zperf_msg
ZPERFMQ_EXPORT void
    zperf_msg_destroy (zperf_msg_t **self_p);

//  Self test of this class
ZPERFMQ_EXPORT void
    zperf_msg_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
