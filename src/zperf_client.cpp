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
    zsys_init();
    zsys_debug ("test zperf_client: ");

    //  @selftest
    zactor_t *server = zactor_new (zperf_server, (char*)"zperf-server");
    assert(server);
    if (verbose) {
        zstr_send (server, "VERBOSE");
    }
    zstr_sendx (server, "BIND", "tcp://127.0.0.1:5678", NULL);

    zperf_client_t *client = zperf_client_new ();
    assert(client);
    zperf_client_set_verbose(client, verbose);

    int rc = 0;

    rc = zperf_client_say_hello(client, "zperf-client", "tcp://127.0.0.1:5678");
    assert (rc == 0);

    rc = zperf_client_create_perf(client, ZMQ_REQ);
    assert(rc == 0);
    char* yodel = zsys_sprintf("%s", zperf_client_ident(client));
    assert (yodel);

    rc = zperf_client_create_perf(client, ZMQ_REP);
    assert(rc == 0);
    char* echo = zsys_sprintf("%s", zperf_client_ident(client));
    assert (echo);    
    if (verbose) {
        zsys_debug("yodel ident is %s", yodel);
        zsys_debug(" echo ident is %s", echo);
    }

    rc = zperf_client_request_borc(client, echo, "BIND", "tcp://127.0.0.1:*");
    assert(rc == 0);
    const char* ep = zperf_client_endpoint(client);

    zsys_debug("requesting CONNECT for %s to %s", yodel, ep);
    rc = zperf_client_request_borc(client, yodel, "CONNECT", ep);
    assert(rc == 0);

    
    

    zperf_client_destroy (&client);
    zsys_debug("client destroyed");
    zactor_destroy (&server);
    zstr_free(&echo);
    zstr_free(&yodel);    
    //  @end
    printf ("OK\n");
}


//  ---------------------------------------------------------------------------
//  set_nickname
//

static void
set_nickname (client_t *self)
{
    zsys_debug("client nickname is %s", self->args->nickname);
    zperf_msg_set_nickname(self->message, self->args->nickname);
}


//  ---------------------------------------------------------------------------
//  use_connect_timeout
//

static void
use_connect_timeout (client_t *self)
{
    engine_set_timeout (self, self->args->timeout);
}


//  ---------------------------------------------------------------------------
//  connect_to_server
//

static void
connect_to_server (client_t *self)
{
    if (zsock_connect (self->dealer, "%s", self->args->endpoint)) {
        engine_set_exception (self, bad_endpoint_event);
        zsys_warning ("could not connect to %s", self->args->endpoint);
    }
}


//  ---------------------------------------------------------------------------
//  signal_bad_endpoint
//

static void
signal_bad_endpoint (client_t *self)
{
    zsock_send (self->cmdpipe, "sis", "FAILURE", -1, "Bad server endpoint");
}


//  ---------------------------------------------------------------------------
//  signal_connected
//

static void
signal_connected (client_t *self)
{
    const char *nickname = zperf_msg_nickname (self->message);
    zsock_send (self->cmdpipe, "sis", "CONNECTED", 0, nickname);
}


//  ---------------------------------------------------------------------------
//  client_is_connected
//

static void
client_is_connected (client_t *self)
{
    // fixme: replicate hydra's heartbeat
    // self->retries = 0;
    // engine_set_connected (self, true);
    // engine_set_timeout (self, self->heartbeat_timer);
}


//  ---------------------------------------------------------------------------
//  set_perf_stype
//

static void
set_perf_stype (client_t *self)
{
    zsys_debug("stype is %d", self->args->stype);
    zperf_msg_set_stype(self->message, self->args->stype);
}


//  ---------------------------------------------------------------------------
//  set_perf_ident
//

static void
set_perf_ident (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  remember_perf
//

static void
remember_perf (client_t *self)
{
    // note: not actually remembering anything, it's up to caller to hold on to ident.
    zsys_debug("remembering perf %s (not actually remembering anything)",
               zperf_msg_ident(self->message));
}


//  ---------------------------------------------------------------------------
//  signal_got_perf
//

static void
signal_got_perf (client_t *self)
{
    const char* ident = zperf_msg_ident (self->message);
    zsock_send (self->cmdpipe, "ss", "PERF IDENT", ident);

}


//  ---------------------------------------------------------------------------
//  set_info_request
//

static void
set_info_request (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  remember_info
//

static void
remember_info (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  signal_got_info
//

static void
signal_got_info (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  set_ident
//

static void
set_ident (client_t *self)
{
    zperf_msg_set_ident(self->message, self->args->ident);
}


//  ---------------------------------------------------------------------------
//  set_socket_request
//

static void
set_socket_request (client_t *self)
{
    zperf_msg_set_borc(self->message, self->args->borc);
    zperf_msg_set_endpoint(self->message, self->args->endpoint);
}


//  ---------------------------------------------------------------------------
//  signal_socket_request
//

static void
signal_socket_request (client_t *self)
{
    const char* ep = zperf_msg_endpoint(self->message);
    zsock_send (self->cmdpipe, "ss", "PERF ENDPOINT", ep);
}


//  ---------------------------------------------------------------------------
//  set_measurement_request
//

static void
set_measurement_request (client_t *self)
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
//  signal_success
//

static void
signal_success (client_t *self)
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





//  ---------------------------------------------------------------------------
//  msg_info_to_caller
//

static void
msg_info_to_caller (client_t *self)
{
}


//  ---------------------------------------------------------------------------
//  msg_results_to_caller
//

static void
msg_results_to_caller (client_t *self)
{
}

