/*  =========================================================================
    zpthr - A throughput measure

    GPL 3.0
    =========================================================================
*/

/*
@header
    zpthr - A throughput measure
@discuss
@end
*/

#include "zperfmq_classes.hpp"

//  Structure of our actor

struct _zpthr_t {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?
    //  Declare properties
    zsock_t* sock;
};


//  --------------------------------------------------------------------------
//  Create a new zpthr instance

static zpthr_t *
zpthr_new (zsock_t *pipe, void *args)
{
    zpthr_t *self = (zpthr_t *) zmalloc (sizeof (zpthr_t));
    assert (self);

    //  Initialize properties
    const char* sock_name = (const char*) args;
    if (!sock_name) {
        sock_name = "PULL";
    }
    if (streq(sock_name, "PULL")) {
        self->sock = zsock_new(ZMQ_PULL);
    }
    else if (streq(sock_name, "SUB")) {
        self->sock = zsock_new(ZMQ_SUB);
    }
    else {
        zsys_error("zpthr: uknown socket type: \"%s\"", sock_name);
        free (self);
        return 0;
    }

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zpthr instance

static void
zpthr_destroy (zpthr_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zpthr_t *self = *self_p;

        //  Free actor properties
        zsock_destroy(&self->sock);

        //  Free object itself
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}


//  Start this actor. Return a value greater or equal to zero if initialization
//  was successful. Otherwise -1.

static int
zpthr_start (zpthr_t *self, int nmsgs)
{
    assert (self);

    int64_t totdata=0;

    void* watch = NULL;

    for (int count=0; count<nmsgs; ++count) {
        int got_count = 0;
        zframe_t* frame = NULL;
        int rc = zsock_recv(self->sock, "if", &got_count, &frame);
        assert (rc == 0);
        assert(count == got_count);

        if (!watch) {
            watch = zmq_stopwatch_start ();
        }

        size_t siz = zframe_size(frame);
        totdata += siz;
        zframe_destroy(&frame);
        // fixme: track skipped counts
    }
    const int64_t elapsed = zmq_stopwatch_stop(watch);

    zsock_send(self->pipe, "si88", "START", nmsgs, totdata, elapsed);

    return 0;
}


//  Stop this actor. Return a value greater or equal to zero if stopping
//  was successful. Otherwise -1.

static int
zpthr_stop (zpthr_t *self)
{
    assert (self);

    //  TODO: Add shutdown actions

    return 0;
}


//  Here we handle incoming message from the node

static void
zpthr_recv_api (zpthr_t *self)
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
        zpthr_start (self, nmsgs);
    }
    else if (streq (command, "STOP")) {
        zpthr_stop (self);
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
zpthr_actor (zsock_t *pipe, void *args)
{
    zpthr_t * self = zpthr_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    while (!self->terminated) {
        zsock_t *which = (zsock_t *) zpoller_wait (self->poller, 0);
        if (which == self->pipe)
            zpthr_recv_api (self);
       //  Add other sockets when you need them.
    }
    zpthr_destroy (&self);
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
zpthr_test (bool verbose)
{
    zsys_init();
    zsys_info ("testing zpthr: ");
    //  @selftest
    //  Simple create/destroy test
    const char* ztype = "PULL";
    zactor_t *zpthr = zactor_new (zpthr_actor, (void*)ztype);
    assert (zpthr);

    if (verbose) {
        zstr_send(zpthr, "VERBOSE");
    }

    zsock_t* spray = zsock_new(ZMQ_PUSH);
    assert(spray);
    zsock_bind(spray, "tcp://127.0.0.1:*");
    const char* endpoint = zsock_endpoint(spray);
    int rc = zsock_send(zpthr, "ss", "CONNECT", endpoint);
    assert(rc == 0);
    char* got_endpoint=0;
    int rc2 = zsock_recv(zpthr,"ssi", NULL, &got_endpoint, &rc);
    assert(rc2 == 0);
    assert(rc == 0);
    assert(streq(got_endpoint, endpoint));
    free(got_endpoint);
    
    const int nmsgs = 10000;
    const size_t msgsize = 1<<18;
    rc = zsock_send(zpthr, "si8", "START", nmsgs);
    assert(rc == 0);

    zframe_t* frame = zframe_new(NULL, msgsize);
    memset (zframe_data(frame), 0, zframe_size(frame));

    for (int count=0; count<nmsgs; ++count) {
        zsock_send(spray, "if", count, frame);        
    }
    zframe_destroy(&frame);

    int got_nmsgs = 0;
    size_t got_totdata = 0;
    int64_t dtus = 0;

    rc = zsock_recv(zpthr, "si88", NULL, &got_nmsgs, &got_totdata, &dtus);
    assert (rc == 0);
    assert (nmsgs == got_nmsgs);
    assert (msgsize*nmsgs == got_totdata);
    const double dtsec = 1e-6*dtus;
    zsys_info("%d msgs took %.3fs, %.3f Hz, %.3f Gbps",
              nmsgs, dtsec, nmsgs/dtsec, 8e-9*got_totdata/dtsec);

    zsock_destroy (&spray);
    zactor_destroy (&zpthr);
    //  @end

    printf ("OK\n");
}
