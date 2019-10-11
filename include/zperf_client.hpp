/*  =========================================================================
    zperf_client - Zperf Client

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: zperf_client.xml, or
     * The code generation script that built this file: zproto_client_c
    ************************************************************************
    LGPL 3.0
    =========================================================================
*/

#ifndef ZPERF_CLIENT_H_INCLUDED
#define ZPERF_CLIENT_H_INCLUDED

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
#ifndef ZPERF_CLIENT_T_DEFINED
typedef struct _zperf_client_t zperf_client_t;
#define ZPERF_CLIENT_T_DEFINED
#endif

//  @interface
//  Create a new zperf_client, return the reference if successful, or NULL
//  if construction failed due to lack of available memory.
zperf_client_t *
    zperf_client_new (void);

//  Destroy the zperf_client and free all memory used by the object.
void
    zperf_client_destroy (zperf_client_t **self_p);

//  Return actor, when caller wants to work with multiple actors and/or
//  input sockets asynchronously.
zactor_t *
    zperf_client_actor (zperf_client_t *self);

//  Return message pipe for asynchronous message I/O. In the high-volume case,
//  we send methods and get replies to the actor, in a synchronous manner, and
//  we send/recv high volume message data to a second pipe, the msgpipe. In
//  the low-volume case we can do everything over the actor pipe, if traffic
//  is never ambiguous.
zsock_t *
    zperf_client_msgpipe (zperf_client_t *self);

//  Return true if client is currently connected, else false. Note that the
//  client will automatically re-connect if the server dies and restarts after
//  a successful first connection.
bool
    zperf_client_connected (zperf_client_t *self);

//  Request a perf to be created. Returned perf ident is needed for any subsequent
//  set_socket or set_measurement method calls .
//  Returns >= 0 if successful, -1 if interrupted.
int
    zperf_client_create_perf (zperf_client_t *self, const char *mtype, const char *stype);

//  Request that a measurement socket be opened on the given endpoint where action
//  is bind or connect. The ident comes from a create_perf() call.
//  Returns >= 0 if successful, -1 if interrupted.
int
    zperf_client_set_socket (zperf_client_t *self, const char *ident, const char *action, const char *endpoint);

//  Set a measurement to be performed. The ident comes from a create_perf() call.
//  Calling this before successfully setting a socket is pointless.
//  Returns >= 0 if successful, -1 if interrupted.
int
    zperf_client_set_measurement (zperf_client_t *self, const char *ident, uint32_t nmsgs, uint64_t msgsize, uint32_t timeout);

//  Return last received ident
const char *
    zperf_client_ident (zperf_client_t *self);

//  Return last received ident and transfer ownership to caller
const char *
    zperf_client_get_ident (zperf_client_t *self);

//  Return last received nmsgs
uint32_t
    zperf_client_nmsgs (zperf_client_t *self);

//  Return last received msgsize
uint64_t
    zperf_client_msgsize (zperf_client_t *self);

//  Return last received timeout
uint32_t
    zperf_client_timeout (zperf_client_t *self);

//  Return last received time_us
uint64_t
    zperf_client_time_us (zperf_client_t *self);

//  Return last received cpu_us
uint64_t
    zperf_client_cpu_us (zperf_client_t *self);

//  Return last received noos
uint32_t
    zperf_client_noos (zperf_client_t *self);

//  Return last received nbytes
uint64_t
    zperf_client_nbytes (zperf_client_t *self);

//  Return last received status
int
    zperf_client_status (zperf_client_t *self);

//  Return last received reason
const char *
    zperf_client_reason (zperf_client_t *self);

//  Return last received reason and transfer ownership to caller
const char *
    zperf_client_get_reason (zperf_client_t *self);

//  Enable verbose tracing (animation) of state machine activity.
void
    zperf_client_set_verbose (zperf_client_t *self, bool verbose);

//  Self test of this class
void
    zperf_client_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
