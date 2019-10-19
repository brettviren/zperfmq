/*  =========================================================================
    zpnode - An actor managing a Zyre node a zperf client and/or server.

    LGPL 3.0
    =========================================================================
*/

/*
@header
    zpnode - An actor managing a Zyre node a zperf client and/or server.
@discuss
@end
*/

#include "zperfmq_classes.hpp"

#define ZPERF_SERVER_HEADER "X-ZPERF-SERVER"

//  Structure of our actor

struct _zpnode_t {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?
    //  TODO: Declare properties

    const char* name;
    zyre_t* zyre;
    bool started;
    zactor_t* server;
    zperf_client_t* client;
};


//  --------------------------------------------------------------------------
//  Create a new zpnode instance

static zpnode_t *
zpnode_new (zsock_t *pipe, void *args)
{
    zpnode_t *self = (zpnode_t *) zmalloc (sizeof (zpnode_t));
    assert (self);

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);

    //  Initialize properties
    self->name = (const char*)args;
    self->zyre = zyre_new(self->name);
    self->server = NULL;
    self->client = NULL;
    
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zpnode instance

static void
zpnode_destroy (zpnode_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zpnode_t *self = *self_p;

        //  Free actor properties
        zyre_destroy(&self->zyre);
        if (self->server) {
            zactor_destroy(&self->server);
        }
        if (self->client) {
            zperf_client_destroy(&self->client);
        }
        //  Free object itself
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}


//  Start this actor. Return a value greater or equal to zero if initialization
//  was successful. Otherwise -1.

static int
zpnode_start (zpnode_t *self)
{
    assert (self);
    assert (!self->started);

    if (self->verbose) {
        zsys_debug("%s: starting", self->name);
    }
    if (zyre_start (self->zyre) == 0) {
        zpoller_add (self->poller, zyre_socket (self->zyre));
        self->started = true;
    }

    return 0;
}


//  Stop this actor. Return a value greater or equal to zero if stopping
//  was successful. Otherwise -1.

static int
zpnode_stop (zpnode_t *self)
{
    assert (self);

    //  TODO: Add shutdown actions

    return 0;
}

static int
zpnode_server (zpnode_t *self, const char* nickname)
{
    assert (self);
    if (self->server) {
        zsys_error("%s: server already created");
        return -1;
    }

    self->server = zactor_new(zperf_server, (void*)nickname);
    if (self->verbose) {
        zstr_send (self->server, "VERBOSE");
    }
    zpoller_add(self->poller, zactor_sock(self->server));

    if (self->verbose) {
        zsys_debug("%s: make server %s", self->name, nickname);
    }
    return 0;
}

static int
zpnode_client (zpnode_t *self)
{
    assert (self);
    if (self->client) {
        zsys_error("%s: client already created");
        return -1;
    }

    self->client = zperf_client_new();
    if (self->verbose) {
        zstr_send (self->client, "VERBOSE");
    }
    zpoller_add(self->poller, zperf_client_msgpipe(self->client));

    if (self->verbose) {
        zsys_debug("%s: make client", self->name);
    }
    return 0;
}


//  Here we handle incoming message from the node

static void
zpnode_recv_api (zpnode_t *self)
{
    //  Get the whole message of the pipe in one go
    zmsg_t *request = zmsg_recv (self->pipe);
    if (!request)
       return;        //  Interrupted

    char *command = zmsg_popstr (request);
    if (self->verbose) {
        zsys_debug("%s: command: %s", self->name, command);
    }
    if (streq (command, "START")) {
        zpnode_start (self);
    }
    else if (streq (command, "STOP")) {
        zpnode_stop (self);
    }
    else if (streq (command, "SERVER")) {
        char* nickname = zmsg_popstr(request);
        zpnode_server(self, nickname);
        free(nickname);
    }
    else if (streq (command, "CLIENT")) {
        zpnode_client(self);
    }
    else if (streq (command, "VERBOSE")) {
        self->verbose = true;
        zyre_set_verbose(self->zyre);
    }
    else if (streq (command, "$TERM")) {
        //  The $TERM command is send by zactor_destroy() method
        self->terminated = true;
    }
    else {
        zsys_error ("%s: invalid command '%s'", self->name, command);
        assert (false);
    }
    zstr_free (&command);
    zmsg_destroy (&request);
}

static void
s_self_handle_zyre (zpnode_t *self)
{
    zyre_event_t *event = zyre_event_new (self->zyre);
    zsys_debug("%s: peer %s, ID %s, event: %s",
               self->name,
               zyre_event_peer_name(event),
               zyre_event_peer_uuid(event),
               zyre_event_type(event));


    if (streq(zyre_event_type (event), "ENTER")) {
        zsys_debug("%s: peer %s, ID %s enter",
                   self->name,
                   zyre_event_peer_name(event),
                   zyre_event_peer_uuid(event));
        const char *endpoint = zyre_event_header (event, ZPERF_SERVER_HEADER);
        if (endpoint) {
            zsys_debug("%s: see zper_server at %s",
                       self->name, endpoint);
        }
    }
    else if (streq(zyre_event_type (event), "EXIT")) {
        zsys_debug("%s: peer %s, ID %s exit",
                   self->name,
                   zyre_event_peer_name(event),
                   zyre_event_peer_uuid(event));
    }
    zyre_event_destroy (&event);
}

static void
s_self_handle_server (zpnode_t *self)
{
    zmsg_t *request = zmsg_recv (zactor_sock(self->server));
    char *command = zmsg_popstr (request);
    if (self->verbose) {
        zsys_debug("%s: server sends: %s", self->name, command);
    }
}

static void
s_self_handle_client (zpnode_t *self)
{
    zperf_msg_t* zpm = zperf_msg_new();
    zperf_msg_recv(zpm, zperf_client_msgpipe(self->client));

    if (self->verbose) {
        zsys_debug("%s: client sends msg", self->name);
        zperf_msg_print(zpm);
    }

    zperf_msg_destroy(&zpm);
}

//  --------------------------------------------------------------------------
//  This is the actor which runs in its own thread.

void
zpnode_actor (zsock_t *pipe, void *args)
{
    zpnode_t * self = zpnode_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    while (!self->terminated) {
        zsock_t *which = (zsock_t *) zpoller_wait (self->poller, 0);
        if (!which) {
            continue;
        }
        if (which == self->pipe) {
            zpnode_recv_api (self);
            continue;
        }
        if (which == zyre_socket(self->zyre)) {
            s_self_handle_zyre(self);
            continue;
        }
        if (self->server && which == zactor_sock(self->server)) {
            s_self_handle_server(self);
            continue;
        }
        if (self->client && which == zperf_client_msgpipe(self->client)) {
            s_self_handle_client(self);
            continue;
        }

        zsys_error("%s: unexpected socket", self->name);
       //  Add other sockets when you need them.
    }
    zpnode_destroy (&self);
}

//  --------------------------------------------------------------------------
//  Self test of this actor.

// If your selftest reads SCMed fixture data, please keep it in
// src/selftest-ro; if your test creates filesystem objects, please
// do so under src/selftest-rw.
// The following pattern is suggested for C selftest code:
//    char *filename = NULL;
//    filename = zsys_sprintf ("%s/%s", SELFTEST_DIR_RO, "mytemplate.file");
//    assert (filename);
//    ... use the "filename" for I/O ...
//    zstr_free (&filename);
// This way the same "filename" variable can be reused for many subtests.
#define SELFTEST_DIR_RO "src/selftest-ro"
#define SELFTEST_DIR_RW "src/selftest-rw"

void
zpnode_test (bool verbose)
{
    zsys_init();
    zsys_debug("test zpnode: ");
    //  @selftest
    //  Simple create/destroy test
    zactor_t *node1 = zactor_new (zpnode_actor, (void*)"zpnode-test1");
    assert (node1);
    zactor_t *node2 = zactor_new (zpnode_actor, (void*)"zpnode-test2");
    assert (node2);

    if (verbose) {
        zstr_send(node1, "VERBOSE");
        zstr_send(node2, "VERBOSE");
    }

    zstr_send(node1, "START");
    zstr_send(node2, "START");

    zclock_sleep(1000);
    zsock_send(node1, "ss", "SERVER", "zperf-server1");
    zsock_send(node2, "ss", "CLIENT", "zperf-client2");
    

    zstr_send(node2, "STOP");
    zstr_send(node1, "STOP");

    zactor_destroy (&node2);
    zactor_destroy (&node1);
    //  @end

    printf ("OK\n");
}
