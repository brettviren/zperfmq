/*  =========================================================================
    zperf_server - class description

    LGPL 3.0
    =========================================================================
*/

#ifndef ZPERF_SERVER_H_INCLUDED
#define ZPERF_SERVER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Create a new zperf_server
ZPERFMQ_EXPORT zperf_server_t *
    zperf_server_new (void);

//  Destroy the zperf_server
ZPERFMQ_EXPORT void
    zperf_server_destroy (zperf_server_t **self_p);

//  Self test of this class
ZPERFMQ_EXPORT void
    zperf_server_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
