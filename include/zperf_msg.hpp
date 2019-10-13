/*  =========================================================================
    zperf_msg - The Zperf Protocol

    Codec header for zperf_msg.

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: zperf_msg.xml, or
     * The code generation script that built this file: zproto_codec_c
    ************************************************************************
    LGPL 3.0
    =========================================================================
*/

#ifndef ZPERF_MSG_H_INCLUDED
#define ZPERF_MSG_H_INCLUDED

/*  These are the zperf_msg messages:

    HELLO - Create a perf on the server associated with the client.
        nickname            string      Client nickname

    HELLO_OK - Create a perf on the server associated with the client.
        nickname            string      Client nickname

    CREATE -
        stype               number 4    Socket type

    LOOKUP -
        ident               number 8    ID for the perf instance

    PERF_OK -
        ident               number 8    ID for the perf instance
        stype               number 4    Socket type
        endpoints           list of string  Socket endpoints

    SOCKET - Bind or connect the measurement socket to the address.
        ident               number 8    ID for the perf instance
        action              string      Bind or Connect
        endpoint            string      Address

    SOCKET_OK - Bind or connect the measurement socket to the address.
        ident               number 8    ID for the perf instance
        action              string      Bind or Connect
        endpoint            string      Address

    MEASURE - Initiate a measurement.
        ident               number 8    ID for the perf instance
        measure             string      Measurement type
        nmsgs               number 4    Number of messages
        msgsize             number 8    Message size in bytes
        timeout             number 4    Timeout in msec

    RESULT - The results of a measurement.
        ident               number 8    ID for the perf instance
        measure             string      Measurement type
        nmsgs               number 4    Number of messages
        msgsize             number 8    Message size in bytes
        timeout             number 4    Timeout in millisec
        time_us             number 8    Time elapsed in microseconds
        cpu_us              number 8    CPU time used (user+system) in microseconds
        noos                number 4    Number of out-of-order messages
        nbytes              number 8    Number of bytes processed

    PING - Client pings the server. Server replies with PING-OK, or ERROR with status
COMMAND-INVALID if the client is not recognized (e.g. after a server restart
or network recovery).

    PING_OK - Server replies to a client ping.

    GOODBYE - Close the connection politely

    GOODBYE_OK - Handshake a connection close

    ERROR - Command failed for some specific reason
        status              number 2    3-digit status code
        reason              string      Printable explanation
*/

#define ZPERF_MSG_SUCCESS                   200
#define ZPERF_MSG_STORED                    201
#define ZPERF_MSG_DELIVERED                 202
#define ZPERF_MSG_NOT_DELIVERED             300
#define ZPERF_MSG_CONTENT_TOO_LARGE         301
#define ZPERF_MSG_TIMEOUT_EXPIRED           302
#define ZPERF_MSG_CONNECTION_REFUSED        303
#define ZPERF_MSG_RESOURCE_LOCKED           400
#define ZPERF_MSG_ACCESS_REFUSED            401
#define ZPERF_MSG_NOT_FOUND                 404
#define ZPERF_MSG_COMMAND_INVALID           500
#define ZPERF_MSG_NOT_IMPLEMENTED           501
#define ZPERF_MSG_INTERNAL_ERROR            502

#define ZPERF_MSG_HELLO                     1
#define ZPERF_MSG_HELLO_OK                  2
#define ZPERF_MSG_CREATE                    3
#define ZPERF_MSG_LOOKUP                    4
#define ZPERF_MSG_PERF_OK                   5
#define ZPERF_MSG_SOCKET                    6
#define ZPERF_MSG_SOCKET_OK                 7
#define ZPERF_MSG_MEASURE                   8
#define ZPERF_MSG_RESULT                    9
#define ZPERF_MSG_PING                      10
#define ZPERF_MSG_PING_OK                   11
#define ZPERF_MSG_GOODBYE                   12
#define ZPERF_MSG_GOODBYE_OK                13
#define ZPERF_MSG_ERROR                     14

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
#ifndef ZPERF_MSG_T_DEFINED
typedef struct _zperf_msg_t zperf_msg_t;
#define ZPERF_MSG_T_DEFINED
#endif

//  @interface
//  Create a new empty zperf_msg
zperf_msg_t *
    zperf_msg_new (void);

//  Create a new zperf_msg from zpl/zconfig_t *
zperf_msg_t *
    zperf_msg_new_zpl (zconfig_t *config);

//  Destroy a zperf_msg instance
void
    zperf_msg_destroy (zperf_msg_t **self_p);

//  Create a deep copy of a zperf_msg instance
zperf_msg_t *
    zperf_msg_dup (zperf_msg_t *other);

//  Receive a zperf_msg from the socket. Returns 0 if OK, -1 if
//  the read was interrupted, or -2 if the message is malformed.
//  Blocks if there is no message waiting.
int
    zperf_msg_recv (zperf_msg_t *self, zsock_t *input);

//  Send the zperf_msg to the output socket, does not destroy it
int
    zperf_msg_send (zperf_msg_t *self, zsock_t *output);



//  Print contents of message to stdout
void
    zperf_msg_print (zperf_msg_t *self);

//  Export class as zconfig_t*. Caller is responsibe for destroying the instance
zconfig_t *
    zperf_msg_zpl (zperf_msg_t *self, zconfig_t* parent);

//  Get/set the message routing id
zframe_t *
    zperf_msg_routing_id (zperf_msg_t *self);
void
    zperf_msg_set_routing_id (zperf_msg_t *self, zframe_t *routing_id);

//  Get the zperf_msg id and printable command
int
    zperf_msg_id (zperf_msg_t *self);
void
    zperf_msg_set_id (zperf_msg_t *self, int id);
const char *
    zperf_msg_command (zperf_msg_t *self);

//  Get/set the nickname field
const char *
    zperf_msg_nickname (zperf_msg_t *self);
void
    zperf_msg_set_nickname (zperf_msg_t *self, const char *value);

//  Get/set the stype field
uint32_t
    zperf_msg_stype (zperf_msg_t *self);
void
    zperf_msg_set_stype (zperf_msg_t *self, uint32_t stype);

//  Get/set the ident field
uint64_t
    zperf_msg_ident (zperf_msg_t *self);
void
    zperf_msg_set_ident (zperf_msg_t *self, uint64_t ident);


//  Get/set the action field
const char *
    zperf_msg_action (zperf_msg_t *self);
void
    zperf_msg_set_action (zperf_msg_t *self, const char *value);

//  Get/set the endpoint field
const char *
    zperf_msg_endpoint (zperf_msg_t *self);
void
    zperf_msg_set_endpoint (zperf_msg_t *self, const char *value);

//  Get/set the measure field
const char *
    zperf_msg_measure (zperf_msg_t *self);
void
    zperf_msg_set_measure (zperf_msg_t *self, const char *value);

//  Get/set the nmsgs field
uint32_t
    zperf_msg_nmsgs (zperf_msg_t *self);
void
    zperf_msg_set_nmsgs (zperf_msg_t *self, uint32_t nmsgs);

//  Get/set the msgsize field
uint64_t
    zperf_msg_msgsize (zperf_msg_t *self);
void
    zperf_msg_set_msgsize (zperf_msg_t *self, uint64_t msgsize);

//  Get/set the timeout field
uint32_t
    zperf_msg_timeout (zperf_msg_t *self);
void
    zperf_msg_set_timeout (zperf_msg_t *self, uint32_t timeout);

//  Get/set the time_us field
uint64_t
    zperf_msg_time_us (zperf_msg_t *self);
void
    zperf_msg_set_time_us (zperf_msg_t *self, uint64_t time_us);

//  Get/set the cpu_us field
uint64_t
    zperf_msg_cpu_us (zperf_msg_t *self);
void
    zperf_msg_set_cpu_us (zperf_msg_t *self, uint64_t cpu_us);

//  Get/set the noos field
uint32_t
    zperf_msg_noos (zperf_msg_t *self);
void
    zperf_msg_set_noos (zperf_msg_t *self, uint32_t noos);

//  Get/set the nbytes field
uint64_t
    zperf_msg_nbytes (zperf_msg_t *self);
void
    zperf_msg_set_nbytes (zperf_msg_t *self, uint64_t nbytes);

//  Get/set the status field
uint16_t
    zperf_msg_status (zperf_msg_t *self);
void
    zperf_msg_set_status (zperf_msg_t *self, uint16_t status);

//  Get/set the reason field
const char *
    zperf_msg_reason (zperf_msg_t *self);
void
    zperf_msg_set_reason (zperf_msg_t *self, const char *value);

//  Self test of this class
void
    zperf_msg_test (bool verbose);
//  @end

//  For backwards compatibility with old codecs
#define zperf_msg_dump      zperf_msg_print

#ifdef __cplusplus
}
#endif

#endif
