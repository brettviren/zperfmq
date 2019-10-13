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

struct perfinfo_t {
    zactor_t* actor;
    int stype;
    zhashx_t* endpoints;

    zlist_t* waiting;
};

//  This structure defines the context for each running server. Store
//  whatever properties and structures you need for the server.


struct _server_t {
    //  These properties must always be present in the server_t
    //  and are set by the generated engine; do not modify them!
    zsock_t *pipe;              //  Actor pipe back to caller
    zconfig_t *config;          //  Current loaded configuration

    // The info about the perfs this server manages.  The key is
    // simply the memory address and that is returend to the creating
    // client as the "ident".  Any client knowing it may also access
    // the perf.
    // 
    // FIXME: Currently nothing destroys these other than death of the
    // server.
    zhashx_t* perfinfos;

    // the perfinfo currently being used (communication between actions).
    perfinfo_t* perfinfo;
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

    //  Specific properties for this application

};


//  Include the generated server engine
#include "zperf_server_engine.inc"


// Destroy an elelment of the perfs hash

static void
s_perfinfo_destroy(perfinfo_t** self_p)
{
    assert (self_p);
    if (! *self_p) { return;}
    perfinfo_t* self = *self_p;
    zactor_destroy(&self->actor);
    zhashx_destroy(&self->endpoints);
    *self_p = 0;
}


//  Allocate properties and structures for a new server instance.
//  Return 0 if OK, or -1 if there was an error.

static int
server_initialize (server_t *self)
{
    //  Construct properties here
    self->perfinfos = zhashx_new();
    zhashx_set_destructor(self->perfinfos, (czmq_destructor*) s_perfinfo_destroy);
    return 0;
}

//  Free properties and structures for a server instance

static void
server_terminate (server_t *self)
{
    //  Destroy properties here
    zhashx_destroy(&self->perfinfos);
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
    perfinfo_t* pi = (perfinfo_t*) zmalloc (sizeof (perfinfo_t));
    pi->stype = zperf_msg_stype(self->message);
    pi->actor = zactor_new (perf_actor, (void*)(size_t)pi->stype);
    assert(pi->actor);

    // this key lets us find the perfinfo from the actor pipe handler
    void* key = (void*)zactor_sock(pi->actor);
    int rc = zhashx_insert(self->server->perfinfos, key, pi);
    assert(rc);

    self->server->perfinfo = pi;

    const uint64_t ident = (uint64_t)pi;
    zperf_msg_set_ident(self->message, ident);
}


//  ---------------------------------------------------------------------------
//  lookup_perf
//

static void
lookup_perf (client_t *self)
{
    const uint64_t ident = zperf_msg_ident(self->message);

    perfinfo_t* pi = (perfinfo_t*)zhashx_lookup(self->server->perfinfos, (void*)ident);
    if (!pi) {
        // signal failure
        return;
    }

    self->server->perfinfo = pi;
}


//  ---------------------------------------------------------------------------
//  set_perf_info
//

static void
set_perf_info (client_t *self)
{
    // serialize what we know about the endpoint 
    zperf_msg_set_stype(self->message, self->server->perfinfo->stype);
    // fixme: and the endpoints
}


//  ---------------------------------------------------------------------------
//  connect_or_bind
//

static void
connect_or_bind (client_t *self)
{
    const char* borc = zperf_msg_borc(self->message);
    const char* ep = zperf_msg_endpoint(self->message);

    if (! (streq(borc, "BIND") || streq(borc,"CONNECT"))) {
        // fixme: signal error
        return;
    }

    // lookup perf should run before
    perfinfo_t* pi = self->server->perfinfo;

    int rc = zsock_send(pi->actor, "ss", borc, ep);
    assert(rc == 0);

    zlist_push(pi->waiting, (void*)self);
}



//  ---------------------------------------------------------------------------
//  start_measure
//

static void
start_measure (client_t *self)
{
    const char* measure = zperf_msg_measure(self->message);

    bool ok = streq(measure, "ECHO") || streq(measure, "YODEL")
        || streq(measure, "SEND") || streq(measure, "RECV");
    if (!ok) {
        zsys_debug("got unknown measure: %s", measure);
        // fixme: signal error
        return;
    }

    int nmsgs = zperf_msg_nmsgs(self->message);
    size_t msgsize = zperf_msg_msgsize(self->message);
    // timeout is ignored for now

    perfinfo_t* pi = self->server->perfinfo;
    int rc = zsock_send(pi->actor, "si8", measure, nmsgs, msgsize);
    assert(rc == 0);

    zlist_push(pi->waiting, (void*)self);
}

static
int pop_int(zmsg_t* msg)
{
    char *value = zmsg_popstr (msg);
    const int ret = atoi(value);
    free (value);
    return ret;
}

static
int64_t pop_long(zmsg_t *msg)
{
    char *value = zmsg_popstr (msg);
    const int64_t ret = atol(value);
    free (value);
    return ret;
}

static int
s_server_handle_perf (zloop_t* loop, zsock_t* pipe, void* argument)
{
    server_t *self = (server_t *) argument;
    void* key = (void*)pipe;
    perfinfo_t* pi = (perfinfo_t*)zhashx_lookup(self->perfinfos, key);
    self->perfinfo = pi;

    client_t* client = (client_t*)zlist_pop(pi->waiting);

    zmsg_t* request = zmsg_recv(pipe);
    char* command = zmsg_popstr(request);
    if (streq(command, "BIND") || streq(command, "CONNECT")) {
        char* ep = zmsg_popstr(request);
        int port_or_rc = pop_int(request);
        if (port_or_rc < 0) {
            // error
        }
        else {                  // forward to client
            zhashx_insert(pi->endpoints, ep, command);
            zperf_msg_set_borc(client->message, command);
            zperf_msg_set_endpoint(client->message, ep);
            engine_set_next_event(client, socket_return_event);
        }
    }
    else if(streq(command, "ECHO") || streq(command, "YODEL")
            || streq(command, "SEND") || streq(command, "RECV")) {
        // i888i
        zperf_msg_set_nmsgs(client->message, pop_int(request));
        zperf_msg_set_msgsize(client->message, pop_long(request));
        zperf_msg_set_time_us(client->message, pop_long(request));
        zperf_msg_set_cpu_us(client->message, pop_long(request));
        zperf_msg_set_noos(client->message, pop_int(request));
        engine_set_next_event(client, measure_return_event);
    }
    else {
        zsys_warning("Unhandled command: %s", command);
    }

    zstr_free(&command);
    zmsg_destroy(&request);
    return 0;
}


//  ---------------------------------------------------------------------------
//  signal_command_invalid
//

static void
signal_command_invalid (client_t *self)
{

}

