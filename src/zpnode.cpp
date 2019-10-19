/*  =========================================================================
    zpnode - An actor managing a Zyre node and a bundle of zperf clients and servers.

    LGPL 3.0
    =========================================================================
*/

/*
@header
    zpnode - An actor managing a Zyre node and a bundle of zperf clients and servers.
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
        // 
    }
    else if (streq (command, "CLIENT")) {
        // 
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
        if (which == self->pipe) {
            zpnode_recv_api (self);
        }
        else if (which == zyre_socket(self->zyre)) {
            s_self_handle_zyre(self);
        }
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

    zstr_send(node2, "STOP");
    zstr_send(node1, "STOP");

    zactor_destroy (&node2);
    zactor_destroy (&node1);
    //  @end

    printf ("OK\n");
}
