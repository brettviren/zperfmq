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

//  @warning THE FOLLOWING @INTERFACE BLOCK IS AUTO-GENERATED BY ZPROJECT
//  @warning Please edit the model at "api/zperf_node.xml" to make changes.
//  @interface
//  This API is a draft, and may change without notice.
#ifdef ZPERFMQ_BUILD_DRAFT_API
//  *** Draft method, for development use, may change without warning ***
//  Create a new zperf_node.
ZPERFMQ_EXPORT zperf_node_t *
    zperf_node_new (const char *nickname);

//  *** Draft method, for development use, may change without warning ***
//  Destroy the zperf_node.
ZPERFMQ_EXPORT void
    zperf_node_destroy (zperf_node_t **self_p);

//  *** Draft method, for development use, may change without warning ***
//  Create a server in the node.
ZPERFMQ_EXPORT void
    zperf_node_server (zperf_node_t *self, const char *nickname);

//  *** Draft method, for development use, may change without warning ***
//  Self test of this class.
ZPERFMQ_EXPORT void
    zperf_node_test (bool verbose);

#endif // ZPERFMQ_BUILD_DRAFT_API
//  @end

#ifdef __cplusplus
}
#endif

#endif
