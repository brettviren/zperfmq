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
    // simply a string representation of the memory address of the
    // perf actor pipe socket and that is returend to the creating
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
    uint  unique_id;            //  Client identifier 

    //  Specific properties for this application

};


//  Include the generated server engine
#include "zperf_server_engine.inc"

static int
    s_server_handle_perf (zloop_t* loop, zsock_t* pipe, void* argument);


// Destroy an elelment of the perfs hash

static void
s_perfinfo_destroy(perfinfo_t** self_p)
{
    assert (self_p);
    if (! *self_p) { return;}
    perfinfo_t* self = *self_p;
    zactor_destroy(&self->actor);
    zhashx_destroy(&self->endpoints);
    zlist_destroy(&self->waiting);
    *self_p = 0;
}


//  Allocate properties and structures for a new server instance.
//  Return 0 if OK, or -1 if there was an error.

static int
server_initialize (server_t *self)
{
    //  Construct properties here
    self->perfinfos = zhashx_new();
    zhashx_set_destructor(self->perfinfos,
                          (czmq_destructor*) s_perfinfo_destroy);

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
//  create_perf
//

static void
create_perf (client_t *self)
{
    perfinfo_t* pi = (perfinfo_t*) zmalloc (sizeof (perfinfo_t));
    pi->stype = zperf_msg_stype(self->message);
    pi->actor = zactor_new (perf_actor, (void*)(size_t)pi->stype);
    assert(pi->actor);
    pi->endpoints = zhashx_new();
    pi->waiting = zlist_new();

    zsock_t* actor_socket = zactor_sock(pi->actor);
    engine_handle_socket (self->server, actor_socket, s_server_handle_perf);

    // this key lets us find the perfinfo from the actor pipe handler
    char* ident = zsys_sprintf("%lX", actor_socket);
    int rc = zhashx_insert(self->server->perfinfos, ident, pi);
    assert(rc == 0);

    self->server->perfinfo = pi;


    zperf_msg_set_id(self->message, ZPERF_MSG_PERF_OK);
    zperf_msg_set_ident(self->message, ident);
    zstr_free(&ident);
}


//  ---------------------------------------------------------------------------
//  lookup_perf
//

static void
lookup_perf (client_t *self)
{
    const char* ident = zperf_msg_ident(self->message);
    perfinfo_t* pi = (perfinfo_t*)zhashx_lookup(self->server->perfinfos, ident);
    if (!pi) {
        zsys_error("Failed to find perfinfo for %s", ident);
        engine_set_exception(self, exception_event);
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

    zsys_debug("zperf-server:connect_or_bind %s %s", borc, ep);

    if (! (streq(borc, "BIND") || streq(borc,"CONNECT"))) {
        zsys_error("must send BIND or CONNECT for borc, got \"%s\"", borc);
        engine_set_exception(self, exception_event);
        return;
    }

    // lookup perf should run before
    perfinfo_t* pi = self->server->perfinfo;
    assert(pi);

    // zsys_debug("%s %s", borc, ep);
    int rc = zsock_send(pi->actor, "ss", borc, ep);
    assert(rc == 0);

    zlist_append(pi->waiting, (void*)self);
    // zsys_debug("%ld waiting", zlist_size(pi->waiting));
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
        engine_set_exception(self, exception_event);
        return;
    }

    int nmsgs = zperf_msg_nmsgs(self->message);
    size_t msgsize = zperf_msg_msgsize(self->message);
    // timeout is ignored for now

    perfinfo_t* pi = self->server->perfinfo;
    int rc = zsock_send(pi->actor, "si8", measure, nmsgs, msgsize);
    assert(rc == 0);

    zlist_append(pi->waiting, (void*)self);
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
    char* ident = zsys_sprintf("%lX", pipe);
    if (engine_verbose(self)) {
        zsys_debug("zperf-server: handling pipe message from perf %s", ident);
    }
    perfinfo_t* pi = (perfinfo_t*)zhashx_lookup(self->perfinfos, ident);
    assert(pi);
    zstr_free(&ident);

    self->perfinfo = pi;
    client_t* client = (client_t*)zlist_pop(pi->waiting);
    assert(client);

    zmsg_t* request = zmsg_recv(pipe);
    char* command = zmsg_popstr(request);
    if (streq(command, "BIND") || streq(command, "CONNECT")) {
        char* ep = zmsg_popstr(request);
        int port_or_rc = pop_int(request);
        if (engine_verbose(self)) {
            zsys_debug ("zperf-server:      %s %s %d",
                        command, ep, port_or_rc);
        }

        if (port_or_rc < 0) {
            engine_set_exception(client, exception_event);
            return -1; // fixme, leaking some stuff
        }
        else {                  // forward to client
            zhashx_insert(pi->endpoints, ep, command);
            zperf_msg_set_id(client->message, ZPERF_MSG_SOCKET_OK);
            zperf_msg_set_borc(client->message, command);
            zperf_msg_set_endpoint(client->message, ep);
            if (engine_verbose(self)) {
                zperf_msg_print(client->message);
            }
            engine_send_event(client, socket_return_event);
        }
    }
    else if(streq(command, "ECHO") || streq(command, "YODEL")
            || streq(command, "SEND") || streq(command, "RECV")) {
        zperf_msg_set_id(client->message, ZPERF_MSG_RESULT);
        zperf_msg_set_nmsgs(client->message, pop_int(request));
        zperf_msg_set_nbytes(client->message, pop_long(request));
        zperf_msg_set_time_us(client->message, pop_long(request));
        zperf_msg_set_cpu_us(client->message, pop_long(request));
        zperf_msg_set_noos(client->message, pop_int(request));
        if (engine_verbose(self)) {
            zperf_msg_print(client->message);
        }
        engine_send_event(client, measure_return_event);
    }
    else {
        zsys_warning("Unhandled command: %s", command);
    }

    zstr_free(&command);
    zmsg_destroy(&request);

}


//  ---------------------------------------------------------------------------
//  borc_return
//

static void
borc_return (client_t *self)
{
    // if (engine_verbose(self->server)) {
    //     zsys_debug("borc");
    //     zperf_msg_print(self->message);
    // }
}

//  ---------------------------------------------------------------------------
//  signal_command_invalid
//

static void
signal_command_invalid (client_t *self)
{
    zperf_msg_set_status(self->message, ZPERF_MSG_COMMAND_INVALID);
}



//  ---------------------------------------------------------------------------
//  Selftest

void
zperf_server_test (bool verbose)
{
    zsys_init();
    zsys_info ("test zperf_server: ");

    int rc=0;
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

    // hello
    zperf_msg_set_id(request, ZPERF_MSG_HELLO);
    zperf_msg_set_nickname(request, "testclient");
    zperf_msg_send(request, client);
    rc = zperf_msg_recv(request, client);
    assert(rc == 0);
    assert(zperf_msg_id(request) == ZPERF_MSG_HELLO_OK);

    // create
    zperf_msg_set_id(request, ZPERF_MSG_CREATE);
    zperf_msg_set_stype(request, ZMQ_REP);
    zperf_msg_send(request, client);
    rc = zperf_msg_recv(request, client);
    assert(rc == 0);
    assert(zperf_msg_id(request) == ZPERF_MSG_PERF_OK);
    assert(zperf_msg_ident(request));
    if (verbose) {
        zsys_debug("got ident: %s", zperf_msg_ident(request));
    }
    // note: just leave ident untouched for subsequent send/recv

    // bind
    zperf_msg_set_id(request, ZPERF_MSG_SOCKET);
    zperf_msg_set_borc(request, "BIND");
    zperf_msg_set_endpoint(request, "tcp://127.0.0.1:*");
    if (verbose) {
        zperf_msg_print(request);
    }
    rc = zperf_msg_send(request, client);
    assert(rc == 0);
    rc = zperf_msg_recv(request, client);
    if (verbose) {
        zsys_debug("SOCKET rc = %d, back = %d",rc, zperf_msg_id(request));
    }
    assert(rc == 0);
    assert(zperf_msg_id(request) == ZPERF_MSG_SOCKET_OK);
    assert(zperf_msg_ident(request));
    const char* endpoint = zperf_msg_endpoint(request);
    assert(endpoint);
    if (verbose) {
        zsys_debug("endpoint %s", endpoint);
    }

    zsock_t* yodel = zsock_new_req(endpoint);
    assert(yodel);

    const int nmsgs = 1000;
    const size_t msgsize = 1024;

    // launch an echo measurement
    zperf_msg_set_id(request, ZPERF_MSG_MEASURE);
    zperf_msg_set_measure(request, "ECHO");
    zperf_msg_set_nmsgs(request, nmsgs);    
    zperf_msg_set_msgsize(request, msgsize);    
    zperf_msg_set_timeout(request, 0); // fixme: unused for now
    rc = zperf_msg_send(request, client);
    assert (rc == 0);
    
    // supply the yodel
    for (int count=0; count < nmsgs; ++count) {
        zmsg_t* msg = zmsg_new();
        zmsg_addmem(msg, &count, sizeof(int));
        zframe_t* frame = zframe_new(NULL, msgsize);
        assert(frame);
        memset (zframe_data(frame), 0, zframe_size(frame));
        zmsg_append(msg, &frame);
        zmsg_send(&msg, yodel);

        // count
        msg = zmsg_recv(yodel);
        assert(msg);
        frame = zmsg_pop(msg);
        assert(frame);
        assert(4 == zframe_size(frame));
        int got_count = *(int*)zframe_data(frame);
        assert(got_count == count);
        zframe_destroy(&frame);

        // payload
        frame = zmsg_pop(msg);
        assert(frame);
        const size_t this_size = zframe_size (frame);
        assert(msgsize == this_size);
        zframe_destroy(&frame);

        zmsg_destroy(&msg);
    }

    // receive the echo side 
    zperf_msg_recv(request, client);
    if (verbose) {
        zperf_msg_print(request);
    }
    assert(zperf_msg_id(request) == ZPERF_MSG_RESULT);
    assert(zperf_msg_nmsgs(request) == nmsgs);
    assert(zperf_msg_msgsize(request) == msgsize);
    assert(zperf_msg_nbytes(request) == msgsize*nmsgs);
    assert(zperf_msg_time_us(request)/nmsgs < 1000); // really slow lo may fail?

    zperf_msg_destroy (&request);

    zsock_destroy (&yodel);
    zsock_destroy (&client);
    zactor_destroy (&server);
    //  @end
    printf ("OK\n");
}


