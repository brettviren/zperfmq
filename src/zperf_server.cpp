/*  =========================================================================
    zperf_server - zperf_server

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
#include "../include/zperf_server.hpp"

//  ---------------------------------------------------------------------------
//  Forward declarations for the two main classes we use here

typedef struct _server_t server_t;
typedef struct _client_t client_t;

//  This structure defines the context for each running server. Store
//  whatever properties and structures you need for the server.

struct _server_t {
    //  These properties must always be present in the server_t
    //  and are set by the generated engine; do not modify them!
    zsock_t *pipe;              //  Actor pipe back to caller
    zconfig_t *config;          //  Current loaded configuration

    //  TODO: Add any properties you need here
};

//  ---------------------------------------------------------------------------
//  This structure defines the state for each client connection. It will
//  be passed to each action in the 'self' argument.

struct _client_t {
    //  These properties must always be present in the client_t
    //  and are set by the generated engine; do not modify them!
    server_t *server;           //  Reference to parent server
    zperf_msg_t *message;       //  Message in and out
    uint  unique_id;            //  Client identifier (for correlation purpose with the engine)

    //  TODO: Add specific properties for your application
};

//  Include the generated server engine
#include "zperf_server_engine.inc"

//  Allocate properties and structures for a new server instance.
//  Return 0 if OK, or -1 if there was an error.

static int
server_initialize (server_t *self)
{
    ZPROTO_UNUSED(self);
    //  Construct properties here
    return 0;
}

//  Free properties and structures for a server instance

static void
server_terminate (server_t *self)
{
    ZPROTO_UNUSED(self);
    //  Destroy properties here
}

//  Process server API method, return reply message if any

static zmsg_t *
server_method (server_t *self, const char *method, zmsg_t *msg)
{
    ZPROTO_UNUSED(self);
    ZPROTO_UNUSED(method);
    ZPROTO_UNUSED(msg);
    return NULL;
}

//  Apply new configuration.

static void
server_configuration (server_t *self, zconfig_t *config)
{
    ZPROTO_UNUSED(self);
    ZPROTO_UNUSED(config);
    //  Apply new configuration
}

//  Allocate properties and structures for a new client connection and
//  optionally engine_set_next_event (). Return 0 if OK, or -1 on error.

static int
client_initialize (client_t *self)
{
    ZPROTO_UNUSED(self);
    //  Construct properties here
    return 0;
}

//  Free properties and structures for a client connection

static void
client_terminate (client_t *self)
{
    ZPROTO_UNUSED(self);
    //  Destroy properties here
}

//  ---------------------------------------------------------------------------
//  Selftest

void
zperf_server_test (bool verbose)
{
    printf (" * zperf_server: ");
    if (verbose)
        printf ("\n");

    //  @selftest
    zactor_t *server = zactor_new (zperf_server, (char*)"server");
    if (verbose)
        zstr_send (server, "VERBOSE");
    zstr_sendx (server, "BIND", "ipc://@/zperf_server", NULL);

    zsock_t *client = zsock_new (ZMQ_DEALER);
    assert (client);
    zsock_set_rcvtimeo (client, 2000);
    zsock_connect (client, "ipc://@/zperf_server");

    //  TODO: fill this out
    zperf_msg_t *request = zperf_msg_new ();
    zperf_msg_destroy (&request);

    zsock_destroy (&client);
    zactor_destroy (&server);
    //  @end
    printf ("OK\n");
}


//  ---------------------------------------------------------------------------
//  create_perf
//

static void
create_perf (client_t *self)
{

}


//  ---------------------------------------------------------------------------
//  signal_command_invalid
//

static void
signal_command_invalid (client_t *self)
{

}


//  ---------------------------------------------------------------------------
//  connect_or_bind
//

static void
connect_or_bind (client_t *self)
{

}


//  ---------------------------------------------------------------------------
//  take_measurement
//

static void
take_measurement (client_t *self)
{

}
