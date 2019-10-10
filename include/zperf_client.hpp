/*  =========================================================================
    zperf_client - class description

    LGPL 3.0
    =========================================================================
*/

#ifndef ZPERF_CLIENT_H_INCLUDED
#define ZPERF_CLIENT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Create a new zperf_client
ZPERFMQ_EXPORT zperf_client_t *
    zperf_client_new (void);

//  Destroy the zperf_client
ZPERFMQ_EXPORT void
    zperf_client_destroy (zperf_client_t **self_p);

//  Self test of this class
ZPERFMQ_EXPORT void
    zperf_client_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
