/*  =========================================================================
    zpray - A message spray service

    GPL 3.0
    =========================================================================
*/

/*
@header
    zpray - A message spray service
@discuss
@end
*/

#include "zperfmq_classes.hpp"

//  Structure of our actor

struct _zpray_t {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?

    //  Declare properties
    zsock_t* sock;              // spray out this socket
};


//  --------------------------------------------------------------------------
//  Create a new zpray instance

static zpray_t *
zpray_new (zsock_t *pipe, void *args)
{
    zpray_t *self = (zpray_t *) zmalloc (sizeof (zpray_t));
    assert (self);

    //  Initialize properties
    const char* sock_name = (const char*) args;
    if (!sock_name) {
        sock_name = "PUSH";
    }
    if (streq(sock_name, "PUSH")) {
        self->sock = zsock_new(ZMQ_PUSH);
    }
    else if (streq(sock_name, "PUB")) {
        self->sock = zsock_new(ZMQ_PUB);
    }
    else {
        zsys_error("zpray: uknown socket type: \"%s\"", sock_name);
        free (self);
        return 0;
    }

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);
    

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zpray instance

static void
zpray_destroy (zpray_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zpray_t *self = *self_p;

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

static int64_t
zpray_start (zpray_t *self, int nmsgs, size_t msgsize)
{
    assert (self);

    const char* got_endpoint = zsock_endpoint(self->sock);
    if (! got_endpoint) {
        zsys_warning("zpray can not start without a bound endpoint");
        return -1;
    }

    zframe_t* frame = zframe_new(NULL, msgsize);
    memset (zframe_data(frame), 0, zframe_size(frame));


    void* watch = zmq_stopwatch_start();
    for (int count = 0; count<nmsgs; ++count) {
        zsock_send(self->sock, "if", count, frame);
    }
    const int64_t elapsed = zmq_stopwatch_stop(watch);
    zframe_destroy(&frame);

    return elapsed;
}


//  Stop this actor. Return a value greater or equal to zero if stopping
//  was successful. Otherwise -1.

static int
zpray_stop (zpray_t *self)
{
    assert (self);

    //  TODO: Add shutdown actions

    return 0;
}


//  Here we handle incoming message from the node

static void
zpray_recv_api (zpray_t *self)
{
    //  Get the whole message of the pipe in one go
    zmsg_t *request = zmsg_recv (self->pipe);
    if (!request)
       return;        //  Interrupted

    char *command = zmsg_popstr (request);
    if (streq (command, "START")) {
        char *value = zmsg_popstr (request);
        int nmsgs = atoi(value);
        free (value);
        value = zmsg_popstr (request);
        const size_t msgsize = atol(value);
        free (value);
        const int64_t dt = zpray_start (self, nmsgs, msgsize);
        zsock_send(self->pipe, "si88", "START", nmsgs, msgsize, dt);
    }
    else if (streq (command, "STOP")) {
        zpray_stop (self);
    }
    else if (streq (command, "VERBOSE")) {
        self->verbose = true;
    }
    else if (streq (command, "BIND")) {
        char *want_endpoint = zmsg_popstr (request);
        const char* got_endpoint = zsock_endpoint(self->sock);
        if (! got_endpoint) {
            int port = zsock_bind(self->sock, "%s", want_endpoint);
            if (port != -1) {
                got_endpoint = zsock_endpoint(self->sock);
            }
        }
        if (! got_endpoint) {
            zsys_warning ("could not bind to %s", want_endpoint);
            zstr_sendm(self->pipe, "BIND");
            zstr_sendf(self->pipe, "");
        }
        else {
            zstr_sendm(self->pipe, "BIND");
            zstr_sendf(self->pipe, got_endpoint);
        }
        free (want_endpoint);
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
zpray_actor (zsock_t *pipe, void *args)
{
    zpray_t * self = zpray_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    while (!self->terminated) {
        zsock_t *which = (zsock_t *) zpoller_wait (self->poller, 0);
        if (which == self->pipe)
            zpray_recv_api (self);
       //  Add other sockets when you need them.
    }
    zpray_destroy (&self);
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
zpray_test (bool verbose)
{
    zsys_init();
    zsys_info ("testing zpray: ");

    //  @selftest
    //  Simple create/destroy test
    const char* ztype = "PUSH";
    zactor_t *zpray = zactor_new (zpray_actor, (void*)ztype);
    assert (zpray);

    if (verbose) {
        zstr_send(zpray, "VERBOSE");
    }

    zsock_send(zpray, "ss", "BIND", "tcp://127.0.0.1:*");
    char* endpoint=NULL;
    char* cmd = NULL;
    zsock_recv(zpray, "ss", &cmd, &endpoint);
    assert(streq(cmd, "BIND"));
    free(cmd);
    assert(endpoint);
    zsys_info("bound to %s", endpoint);

    const int nmsgs = 10000;
    const size_t msgsize = 1<<18;
    int rc = zsock_send(zpray, "si8", "START", nmsgs, msgsize);
    assert (rc == 0);

    zsock_t* sink = zsock_new(ZMQ_PULL);
    assert(sink);
    int nconn = 1;
    const char* var = getenv("ZPERFMQ_NCONNECTIONS");
    if (var) {
        nconn = atoi(var);
        zsys_info("Using multiple connections (%d)", nconn);
    }
    var = getenv("ZSYS_IO_THREADS");
    if (var) {
        zsys_info("Using multiple I/O threads (%s)", var);
    }
    for (int iconn=0; iconn<nconn; ++iconn) {
        rc = zsock_connect(sink, "%s", endpoint);
        assert(rc==0);
    }
    free(endpoint);
    bool out_of_order = false;
    for (int count=0; count<nmsgs; ++count) {
        int got_count = 0;
        zframe_t* frame = NULL;
        rc = zsock_recv(sink, "if", &got_count, &frame);
        assert (rc == 0);
        if (count != got_count) {
            out_of_order = true;
        }
        //assert(count == got_count);
        assert(zframe_size(frame) == msgsize);
        zframe_destroy(&frame);
    }
    zsock_destroy(&sink);
    if (out_of_order) {
        zsys_warning("got out-of-order messages");
    }

    int64_t dtus=0;
    int got_nmsgs=0;
    size_t got_msgsize=0;
    zsock_recv(zpray, "si88", NULL, &got_nmsgs, &got_msgsize, &dtus);
    assert(got_nmsgs == nmsgs);
    assert(got_msgsize == msgsize);
    double dtsec = dtus*1e-6;
    zsys_info("%d msgs took %.3fs, %.3f Hz, %.3f Gbps",
              nmsgs, dtsec, nmsgs/dtsec, 8e-9*msgsize*nmsgs/dtsec);

    zactor_destroy (&zpray);
    //  @end

    printf ("OK\n");
}
