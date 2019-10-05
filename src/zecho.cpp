/*  =========================================================================
    zecho - A message echo service

    GPL 3.0
    =========================================================================
*/

/*
@header
    zecho - A message echo service
@discuss
@end
*/

#include "zperfmq_classes.hpp"

//  Structure of our actor

struct _zecho_t {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?
    //  Declare properties
    zsock_t *sock;              //  The REP or ROUTER

};


//  --------------------------------------------------------------------------
//  Create a new zecho instance

static zecho_t *
zecho_new (zsock_t *pipe, void *args)
{
    zecho_t *self = (zecho_t *) zmalloc (sizeof (zecho_t));
    assert (self);

    //  Initialize properties
    const char* sock_name = (const char*) args;
    if (!sock_name) {
        sock_name = "REP";
    }
    if (streq(sock_name, "REP")) {
        self->sock = zsock_new(ZMQ_REP);
    }
    else if (streq(sock_name, "ROUTER")) {
        self->sock = zsock_new(ZMQ_ROUTER);
    }
    else {
        zsys_error("zecho: uknown socket type: \"%s\"", sock_name);
        free (self);
        return 0;
    }

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zecho instance

static void
zecho_destroy (zecho_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zecho_t *self = *self_p;

        //  Free actor properties
        zsock_destroy(&self->sock);

        //  Free object itself
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}


//  Start this actor to echo given numer of messages. Return time
//  elapsed in microseconds.  Otherwise -1.

static int64_t
zecho_start (zecho_t *self, int nmsgs)
{
    assert (self);

    const char* got_endpoint = zsock_endpoint(self->sock);
    if (! got_endpoint) {
        zsys_warning("zecho can not start without a bound endpoint");
        return -1;
    }

    void* watch = NULL;
    while (nmsgs) {
        zmsg_t* zmsg = zmsg_recv(self->sock);
        if (!watch) {           // start on first message
            watch = zmq_stopwatch_start();
        }
        zmsg_send(&zmsg, self->sock);
        --nmsgs;
    }

    const int64_t elapsed = zmq_stopwatch_stop(watch);
    return elapsed;
}


//  Stop this actor. Return a value greater or equal to zero if stopping
//  was successful. Otherwise -1.

static int
zecho_stop (zecho_t *self)
{
    assert (self);

    const char* got_endpoint = zsock_endpoint(self->sock);
    if (got_endpoint) {
        zsock_disconnect(self->sock, "%s", got_endpoint);
    }

    return 0;
}


//  Here we handle incoming message from the node

static void
zecho_recv_api (zecho_t *self)
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
        const int64_t dt = zecho_start (self, nmsgs);
        zsock_send(self->pipe, "si8", "START", nmsgs, dt);
    }
    else if (streq (command, "STOP")) {
        zecho_stop (self);
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
zecho_actor (zsock_t *pipe, void *args)
{
    zecho_t * self = zecho_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    while (!self->terminated) {
        zsock_t *which = (zsock_t *) zpoller_wait (self->poller, 0);
        if (which == self->pipe)
            zecho_recv_api (self);
       //  Add other sockets when you need them.
    }
    zecho_destroy (&self);
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
zecho_test (bool verbose)
{
    zsys_init();
    zsys_info ("testing zecho: ");
    //  @selftest
    //  Simple create/destroy test

    //const char* ztype = "ROUTER";
    const char* ztype = "REP";
    zactor_t *zecho = zactor_new (zecho_actor, (void*)ztype);
    assert (zecho);

    if (verbose) {
        zstr_send(zecho, "VERBOSE");
    }

    zsock_send(zecho, "ss", "BIND", "tcp://127.0.0.1:*");
    char* endpoint=NULL;
    char* cmd = NULL;
    zsock_recv(zecho, "ss", &cmd, &endpoint);
    assert(streq(cmd, "BIND"));
    free(cmd);
    assert(endpoint);
    zsys_info("bound to %s", endpoint);

    const int nmsgs = 10000;
    int rc = zsock_send(zecho, "si", "START", nmsgs);
    assert (rc == 0);

    zsock_t* echo = zsock_new_req(endpoint);
    assert(echo);
    free(endpoint);
    for (int count=0; count < nmsgs; ++count) { 
        rc = zstr_send(echo, "hello");
        assert (rc == 0);
        char* reply = zstr_recv(echo);
        assert(streq(reply,"hello"));
        free (reply);
    }
    zsock_destroy(&echo);
    
    int64_t dtus=0;
    int got_nmsgs=0;
    zsock_recv(zecho, "si8", NULL, &got_nmsgs, &dtus);
    assert(got_nmsgs == nmsgs);
    double dtsec = dtus*1e-6;
    zsys_info("%d msgs took %.3fs, %.3f Hz, %.3f us rtt",
              nmsgs, dtsec, nmsgs/dtsec, double(dtus)/nmsgs);

    zactor_destroy (&zecho);
    //  @end

    printf ("OK\n");
}