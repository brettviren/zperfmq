/*  =========================================================================
    perf - A swish army knife of perf services.

It can provide both ends of a fast echo for latency measurements
and a sender or receiver of a fast flow for throughput measurements.

Measurements are initiated via command messages with results
returned.  While a measurement is ongoing the actor pipe is not
serviced.

    LGPL 3.0
    =========================================================================
*/

/*
@header
    perf - A swish army knife of perf services.

It can provide both ends of a fast echo for latency measurements
and a sender or receiver of a fast flow for throughput measurements.

Measurements are initiated via command messages with results
returned.  While a measurement is ongoing the actor pipe is not
serviced.
@discuss
@end
*/

#include "zperfmq_classes.h"

//  Structure of our actor

struct _perf_t {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?
    //  TODO: Declare properties
};


//  --------------------------------------------------------------------------
//  Create a new perf instance

static perf_t *
perf_new (zsock_t *pipe, void *args)
{
    perf_t *self = (perf_t *) zmalloc (sizeof (perf_t));
    assert (self);

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);

    //  TODO: Initialize properties

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the perf instance

static void
perf_destroy (perf_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        perf_t *self = *self_p;

        //  TODO: Free actor properties

        //  Free object itself
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}


//  Start this actor. Return a value greater or equal to zero if initialization
//  was successful. Otherwise -1.

static int
perf_start (perf_t *self)
{
    assert (self);

    //  TODO: Add startup actions

    return 0;
}


//  Stop this actor. Return a value greater or equal to zero if stopping
//  was successful. Otherwise -1.

static int
perf_stop (perf_t *self)
{
    assert (self);

    //  TODO: Add shutdown actions

    return 0;
}


//  Here we handle incoming message from the node

static void
perf_recv_api (perf_t *self)
{
    //  Get the whole message of the pipe in one go
    zmsg_t *request = zmsg_recv (self->pipe);
    if (!request)
       return;        //  Interrupted

    char *command = zmsg_popstr (request);
    if (streq (command, "START"))
        perf_start (self);
    else
    if (streq (command, "STOP"))
        perf_stop (self);
    else
    if (streq (command, "VERBOSE"))
        self->verbose = true;
    else
    if (streq (command, "$TERM"))
        //  The $TERM command is send by zactor_destroy() method
        self->terminated = true;
    else {
        zsys_error ("invalid command '%s'", command);
        assert (false);
    }
    zstr_free (&command);
    zmsg_destroy (&request);
}


//  --------------------------------------------------------------------------
//  This is the actor which runs in its own thread.

void
perf_actor (zsock_t *pipe, void *args)
{
    perf_t * self = perf_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    while (!self->terminated) {
        zsock_t *which = (zsock_t *) zpoller_wait (self->poller, 0);
        if (which == self->pipe)
            perf_recv_api (self);
       //  Add other sockets when you need them.
    }
    perf_destroy (&self);
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
perf_test (bool verbose)
{
    printf (" * perf: ");
    //  @selftest
    //  Simple create/destroy test
    zactor_t *perf = zactor_new (perf_actor, NULL);
    assert (perf);

    zactor_destroy (&perf);
    //  @end

    printf ("OK\n");
}
