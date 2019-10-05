/*  =========================================================================
    zplat - A round-trip latency measure

    GPL 3.0
    =========================================================================
*/

/*
@header
    zplat - A round-trip latency measure
@discuss
@end
*/

#include "zperfmq_classes.hpp"

//  Structure of our actor

struct _zplat_t {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?

    //  Declare properties
    zsock_t* sock;
};


//  --------------------------------------------------------------------------
//  Create a new zplat instance

static zplat_t *
zplat_new (zsock_t *pipe, void *args)
{
    zplat_t *self = (zplat_t *) zmalloc (sizeof (zplat_t));
    assert (self);

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);

    //  Initialize properties
    self->sock = zsock_new(ZMQ_REQ);

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zplat instance

static void
zplat_destroy (zplat_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zplat_t *self = *self_p;

        //  Free actor properties
        zsock_destroy(&self->sock);

        //  Free object itself
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}


//  Start this actor and process nmsgs of the given size.  Return
//  elapsed time in microsconds if successful. Otherwise -1.

static int64_t
zplat_start (zplat_t *self, int nmsgs, size_t message_size)
{
    assert (self);

    //  Add startup actions
    zmq_msg_t msg;
    int rc = zmq_msg_init_size (&msg, message_size);
    if (rc != 0) {
        zsys_error ("zplat: error in zmq_msg_init: %s\n", zmq_strerror (errno));
        return -1;
    }
    memset (zmq_msg_data (&msg), 0, message_size);

    // drop down to libzmq
    void* s = zsock_resolve(self->sock);
    void* watch = NULL;
    while (nmsgs) {
        rc = zmq_sendmsg (s, &msg, 0);
        if (rc < 0) {
            zsys_error ("zplat: error in zmq_sendmsg: %s\n", zmq_strerror (errno));
            return -1;
        }
        if (!watch) {
            watch = zmq_stopwatch_start ();
        }
        rc = zmq_recvmsg (s, &msg, 0);
        if (rc < 0) {
            zsys_error ("zplat: error in zmq_recvmsg: %s\n", zmq_strerror (errno));
            return -1;
        }
        if (zmq_msg_size (&msg) != message_size) {
            zsys_error ("zplat: message of incorrect size received\n");
            return -1;
        }        
        --nmsgs;
    }

    const int64_t elapsed = zmq_stopwatch_stop(watch);

    rc = zmq_msg_close (&msg);
    if (rc != 0) {
        zsys_error ("zplat: error in zmq_msg_close: %s\n", zmq_strerror (errno));
        return -1;
    }

    return elapsed;
}


//  Stop this actor. Return a value greater or equal to zero if stopping
//  was successful. Otherwise -1.

static int
zplat_stop (zplat_t *self)
{
    assert (self);

    //  TODO: Add shutdown actions

    return 0;
}


//  Here we handle incoming message from the node

static void
zplat_recv_api (zplat_t *self)
{
    //  Get the whole message of the pipe in one go
    zmsg_t *request = zmsg_recv (self->pipe);
    if (!request)
       return;        //  Interrupted

    char *command = zmsg_popstr (request);
    if (streq (command, "START")) {
        char *value = zmsg_popstr (request);
        const int nmsgs = atoi(value);
        free (value);
        value = zmsg_popstr (request);
        const size_t msgsize = atol(value);
        free (value);
        const int64_t dt = zplat_start (self, nmsgs, msgsize);
        zsock_send(self->pipe, "si88", "START", nmsgs, msgsize, dt);
    }
    else if (streq (command, "STOP")) {
        zplat_stop (self);
    }
    else if (streq (command, "VERBOSE")) {
        self->verbose = true;
    }
    else if (streq (command, "CONNECT")) {
        char *endpoint = zmsg_popstr (request);
        int rc = zsock_connect(self->sock, "%s", endpoint);
        zsock_send(self->pipe, "ssi", "CONNECT", endpoint, rc);        
        free (endpoint);
    }
    else if (streq (command, "$TERM")) {
        //  The $TERM command is send by zactor_destroy() method
        self->terminated = true;
    }
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
zplat_actor (zsock_t *pipe, void *args)
{
    zplat_t * self = zplat_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    while (!self->terminated) {
        zsock_t *which = (zsock_t *) zpoller_wait (self->poller, 0);
        if (which == self->pipe)
            zplat_recv_api (self);
       //  Add other sockets when you need them.
    }
    zplat_destroy (&self);
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
zplat_test (bool verbose)
{
    zsys_init();
    zsys_info ("testing zplat: ");
    //  @selftest
    //  Simple create/destroy test
    zactor_t *zplat = zactor_new (zplat_actor, NULL);
    assert (zplat);

    if (verbose) {
        zstr_send(zplat, "VERBOSE");
    }
    
    zsock_t* echo = zsock_new_rep("tcp://127.0.0.1:*");
    const char* endpoint = zsock_endpoint(echo);
    int rc = zsock_send(zplat, "ss", "CONNECT", endpoint);
    assert(rc == 0);
    char* got_endpoint=0;
    int rc2 = zsock_recv(zplat,"ssi", NULL, &got_endpoint, &rc);
    assert(rc2 == 0);
    assert(rc == 0);
    assert(streq(got_endpoint, endpoint));
    free(got_endpoint);

    const int nmsgs = 10000;
    const size_t msgsize = 1024;
    rc = zsock_send(zplat, "si8", "START", nmsgs, msgsize);
    assert(rc == 0);

    zmq_msg_t msg;
    rc = zmq_msg_init (&msg);
    assert (rc == 0);
    void* s = zsock_resolve(echo);
    for (int count=0; count<nmsgs; ++count) {
        int nbytes = zmq_recvmsg (s, &msg, 0);        
        assert (nbytes == msgsize);
        assert (zmq_msg_size (&msg)  == msgsize);
        nbytes = zmq_sendmsg (s, &msg, 0);
        assert (nbytes == msgsize);
    }
    rc = zmq_msg_close (&msg);
    assert(rc == 0);

    int got_nmsgs = 0;
    size_t got_msgsize = 0;
    int64_t dtus = 0;
    rc = zsock_recv(zplat, "si88", NULL, &got_nmsgs, &got_msgsize, &dtus);
    assert (rc == 0);
    assert (msgsize == got_msgsize);
    assert (nmsgs == got_nmsgs);
    const double dtsec = 1e-6*dtus;
    zsys_info("%d msgs took %.3fs, %.3f Hz, %.3f us rtt",
              nmsgs, dtsec, nmsgs/dtsec, double(dtus)/nmsgs);

    zsock_destroy(&echo);
    zactor_destroy (&zplat);
    //  @end

    printf ("OK\n");
}
