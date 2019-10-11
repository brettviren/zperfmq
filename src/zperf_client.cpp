/*  =========================================================================
    zperf_client - Zperf Client

    LGPL 3.0
    =========================================================================
*/

/*
@header
    Description of class for man page.
@discuss
    Detailed discussion of the class, if any.
@end
*/

#include "zperfmq_classes.hpp"
//  TODO: Change these to match your project's needs
#include "../include/zperf_msg.hpp"
#include "../include/zperf_client.hpp"

//  Forward reference to method arguments structure
typedef struct _client_args_t client_args_t;

//  This structure defines the context for a client connection
typedef struct {
    //  These properties must always be present in the client_t
    //  and are set by the generated engine. The cmdpipe gets
    //  messages sent to the actor; the msgpipe may be used for
    //  faster asynchronous message flows.
    zsock_t *cmdpipe;           //  Command pipe to/from caller API
    zsock_t *msgpipe;           //  Message pipe to/from caller API
    zsock_t *dealer;            //  Socket to talk to server
    zperf_msg_t *message;       //  Message to/from server
    client_args_t *args;        //  Arguments from methods

    //  TODO: Add specific properties for your application
} client_t;

//  Include the generated client engine
#include "zperf_client_engine.inc"

//  Allocate properties and structures for a new client instance.
//  Return 0 if OK, -1 if failed

static int
client_initialize (client_t *self)
{
    return 0;
}

//  Free properties and structures for a client instance

static void
client_terminate (client_t *self)
{
    //  Destroy properties here
}


//  ---------------------------------------------------------------------------
//  Selftest

void
zperf_client_test (bool verbose)
{
    printf (" * zperf_client: ");
    if (verbose)
        printf ("\n");

    //  @selftest
    // TODO: fill this out
    zperf_client_t *client = zperf_client_new ();
    zperf_client_set_verbose(client, verbose);
    zperf_client_destroy (&client);
    //  @end
    printf ("OK\n");
}


//  ---------------------------------------------------------------------------
//  connect_to_server_endpoint
//

static void
connect_to_server_endpoint (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  set_nickname
//

static void
set_nickname (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  use_connect_timeout
//

static void
use_connect_timeout (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  signal_bad_endpoint
//

static void
signal_bad_endpoint (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  signal_success
//

static void
signal_success (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  signal_connected
//

static void
signal_connected (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  client_is_connected
//

static void
client_is_connected (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  set_perf_attributes
//

static void
set_perf_attributes (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  set_socket_request
//

static void
set_socket_request (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  set_measurement_request
//

static void
set_measurement_request (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  remember_perf
//

static void
remember_perf (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  signal_perf_creation
//

static void
signal_perf_creation (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  signal_socket_request
//

static void
signal_socket_request (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  set_results
//

static void
set_results (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  signal_results
//

static void
signal_results (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  check_if_connection_is_dead
//

static void
check_if_connection_is_dead (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  check_status_code
//

static void
check_status_code (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  signal_internal_error
//

static void
signal_internal_error (client_t *self)
{
}
