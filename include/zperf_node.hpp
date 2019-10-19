/*  =========================================================================
    zperf_node - API to a zpnode.

    LGPL 3.0
    =========================================================================
*/

#ifndef ZPERF_NODE_H_INCLUDED
#define ZPERF_NODE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Create a new zperf_node
ZPERFMQ_EXPORT zperf_node_t *
    zperf_node_new (void);

//  Destroy the zperf_node
ZPERFMQ_EXPORT void
    zperf_node_destroy (zperf_node_t **self_p);

//  Self test of this class
ZPERFMQ_EXPORT void
    zperf_node_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
