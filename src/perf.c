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

    //  Declare properties
    zsock_t* sock;              /* The measurement socket */
};


//  --------------------------------------------------------------------------
//  Create a new perf instance, args should hold an integer value
//  giving the socket type.

static perf_t *
perf_new (zsock_t *pipe, void *args)
{
    size_t stype = (size_t)args;
    zsock_t *sock = zsock_new(stype);
    if (!sock) {
        return NULL;
    }
    if (stype == ZMQ_SUB) {
        zsock_set_subscribe(sock, "");
    }


    perf_t *self = (perf_t *) zmalloc (sizeof (perf_t));
    assert (self);

    self->sock = sock;

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);

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

        //  Free actor properties
        zsock_destroy(&self->sock);

        //  Free object itself
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}



// Bind the measurement socket.

static int
perf_bind (perf_t *self, const char* endpoint)
{
    assert (self);

    int rc=0, port = zsock_bind(self->sock, "%s", endpoint);
    if (port <= 0) {
        rc = zsock_send(self->pipe, "ssi", "BIND", endpoint, port);
        assert(rc == 0);
        return -1;
    }
    endpoint = zsock_endpoint(self->sock);
    rc = zsock_send(self->pipe, "ssi", "BIND", endpoint, port);    
    assert(rc == 0);
    return port;
}


// Connect the measurement socket.

static int
perf_connect (perf_t *self, const char* endpoint)
{
    assert (self);

    int rc = zsock_connect(self->sock, "%s", endpoint);
    // CONNECT <address> <rc>
    zsock_send(self->pipe, "ssi", "CONNECT", endpoint, rc);
    return rc;
}

// Run an echo service.  Return results down pipe.

static int
perf_echo (perf_t *self, int nmsgs)
{
    assert (self);

    void* watch = NULL;
    for (int count=0; count<nmsgs; ++count) {
        zmsg_t* msg = zmsg_recv(self->sock);
        if (!watch) {           // start after first yodel received
            watch = zmq_stopwatch_start();
        }
        int rc = zmsg_send(&msg, self->sock);
        assert (rc == 0);
    }

    const int64_t elapsed = zmq_stopwatch_stop(watch);

    // ECHO <nmsgs> <time_us>
    int rc = zsock_send(self->pipe, "si8", "ECHO", nmsgs, elapsed);

    return rc;
}


// Run a latency measurement against an echo service.  Return results
// down pipe.

static int
perf_yodel (perf_t *self, int nmsgs, size_t msgsize)
{
    assert (self);

    // This test currently uses CZMQ level of functions which may add
    // its own latency.  
    zframe_t* frame = zframe_new(NULL, msgsize);
    assert(frame);
    memset (zframe_data(frame), 0, zframe_size(frame));

    int rc=0, noos = 0;
    void* watch = zmq_stopwatch_start();
    for (int count=0; count < nmsgs; ++count) {
        rc = zsock_send(self->sock, "if", count, frame);
        assert(rc == 0);
        zframe_t* got_frame = NULL;
        int got_count = 0;
        rc = zsock_recv(self->sock, "if", &got_count, &got_frame);
        assert (rc == 0);
        if (count != got_count) {
            ++noos;             /* number out-of-sync */
        }
        zframe_destroy(&got_frame);
    }
    const int64_t elapsed = zmq_stopwatch_stop(watch);

    zframe_destroy(&frame);

    // YODEL <nmsgs> <msgsize> <time_us> <noos>
    rc = zsock_send(self->pipe, "si88i", "YODEL",
                    nmsgs, msgsize, elapsed, noos);
    return rc;
}


// Send some data.  Return results down pipe.

static int
perf_send (perf_t *self, int nmsgs, size_t msgsize)
{
    assert (self);

    zframe_t* frame = zframe_new(NULL, msgsize);
    memset (zframe_data(frame), 0, zframe_size(frame));    

    void* watch = zmq_stopwatch_start();
    for (int count = 0; count<nmsgs; ++count) {
        int rc = zsock_send(self->sock, "if", count, frame);
        assert(rc == 0);
    }
    const int64_t elapsed = zmq_stopwatch_stop(watch);

    zframe_destroy(&frame);
    
    // SEND <nmsgs> <msgsize> <time_us>
    int rc = zsock_send(self->pipe, "si88", "SEND",
                        nmsgs, msgsize, elapsed);

    return rc;
}


// Receive some data.  Return results down pipe.

static int
perf_recv (perf_t *self, int nmsgs)
{
    assert (self);

    int64_t totdata=0;
    void* watch = NULL;
    int rc=0, noos=0;
    for (int count = 0; count < nmsgs; ++count) {
        int got_count = 0;
        zframe_t* frame = NULL;
        rc = zsock_recv(self->sock, "if", &got_count, &frame);
        assert (rc == 0);
        if (count != got_count) {
            ++noos;
        }
        if (!watch) {           /* start after first message */
            watch = zmq_stopwatch_start ();
        }

        size_t siz = zframe_size(frame);
        totdata += siz;
        zframe_destroy(&frame);        
    }

    const int64_t elapsed = zmq_stopwatch_stop(watch);

    // RECV <nmsgs> <total_size> <time_us> <noos>
    rc = zsock_send(self->pipe, "si88i", "RECV",
                    nmsgs, totdata, elapsed, noos);

    return rc;
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
    // BIND <address> ->
    // BIND <fq-address> <port#, zero or err>
    if (streq (command, "BIND")) {
        char *endpoint = zmsg_popstr (request);        
        perf_bind (self, endpoint);
        free (endpoint);        
    }
    // CONNECT <address> ->
    // CONNECT <address> <rc>
    else if (streq (command, "CONNECT")) {
        char *endpoint = zmsg_popstr (request);
        perf_connect (self, endpoint);
        free (endpoint);
    }
    // ECHO <nmsgs> ->
    // ECHO <nmsgs> <time_us>
    else if (streq (command, "ECHO")) {
        char *value = zmsg_popstr (request);
        int nmsgs = atoi(value);
        free (value);
        perf_echo (self, nmsgs);
    }
    // YODEL <nmsgs> <msgsize> ->
    // YODEL <nmsgs> <msgsize> <time_us> <noos>
    else if (streq (command, "YODEL")) {
        char *value = zmsg_popstr (request);
        const int nmsgs = atoi(value);
        free (value);
        value = zmsg_popstr (request);
        const size_t msgsize = atol(value);
        free (value);        
        perf_yodel (self, nmsgs, msgsize);
    }
    // SEND <nmsgs> <msgsize> ->
    // SEND <nmsgs> <msgsize> <time_us>
    else if (streq (command, "SEND")) {
        char *value = zmsg_popstr (request);
        int nmsgs = atoi(value);
        free (value);
        value = zmsg_popstr (request);
        const size_t msgsize = atol(value);
        free (value);
        perf_send (self, nmsgs, msgsize);
    }
    // RECV <nmsgs> ->
    // RECV <nmsgs> <total_size> <time_us> <noos>
    else if (streq (command, "RECV")) {
        char *value = zmsg_popstr (request);
        int nmsgs = atoi(value);
        free (value);
        perf_recv (self, nmsgs);
    }
    else if (streq (command, "VERBOSE")) {
        self->verbose = true;
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

static
char* s_bind(zactor_t* zp)
{
    int rc = zsock_send(zp, "ss", "BIND", "tcp://127.0.0.1:*");
    assert(rc == 0);
    char* got_ep=0;
    char* got_cmd=0;
    int rc2 = zsock_recv(zp, "ssi", &got_cmd, &got_ep, &rc);
    assert(rc2 == 0);
    assert(rc >= 0);
    assert(streq(got_cmd, "BIND"));
    assert(got_ep);
    free(got_cmd);
    return got_ep;              /* caller owns */
}

static
void s_conn(zactor_t* zp, const char* ep)
{
    int rc = zsock_send(zp, "ss", "CONNECT", ep);
    assert(rc == 0);
    char* got_cmd = 0;
    char* got_ep = 0;
    int rc2 = zsock_recv(zp, "ssi", &got_cmd, &got_ep, &rc);
    assert(rc2 == 0);
    assert(rc == 0);
    assert(streq("CONNECT", got_cmd));
    assert(streq(ep, got_ep));
    free(got_ep);
    free(got_cmd);
}

static
void s_report(const char* what, int nmsgs, size_t totdat, int64_t time_us)
{
    double time_s = 1e-6*time_us;
    double Gbps = totdat*8e-9/time_s;
    double kHz = 0.001*nmsgs/time_s;
    double lat_us = 1e6*time_s/nmsgs;
    zsys_info("%s: %d msgs (%.3f Gbps) in %.3fs, %.3f kHz, %.3f us/msg",
              what, nmsgs, Gbps, time_s, kHz, lat_us);
}

static
void s_echo_ini(zactor_t* zp, int nmsgs)
{
    int rc = zsock_send(zp, "si", "ECHO", nmsgs);
    assert(rc == 0);
}
static
void s_echo_fin(zactor_t* zp, int nmsgs, size_t msgsize)
{
    int got_nmsgs=0;
    int64_t got_time=0;
    char* got_cmd=0;
    int rc = zsock_recv(zp, "si8", &got_cmd, &got_nmsgs, &got_time);
    assert(rc == 0);
    assert(streq(got_cmd, "ECHO"));
    free(got_cmd);
    assert(nmsgs == got_nmsgs);
    s_report("echo", got_nmsgs, nmsgs*msgsize, got_time);
}
static
void s_yodel(zactor_t* zp, int nmsgs, size_t msgsize)
{
    int rc = zsock_send(zp, "si8", "YODEL", nmsgs, msgsize);
    assert(rc == 0);
    int got_nmsgs=0, got_noos=0;
    int64_t time_us=0, got_msgsize=0;
    char* got_cmd=0;
    rc = zsock_recv(zp, "si88i", &got_cmd,
                    &got_nmsgs, &got_msgsize, &time_us, &got_noos);
    assert(rc == 0);
    assert(streq("YODEL", got_cmd));
    free(got_cmd);
    assert(got_nmsgs == nmsgs);
    assert(got_msgsize == msgsize);
    s_report("yodel", got_nmsgs, nmsgs*msgsize, time_us);
    zsys_info("yodel: %d out-of-order", got_noos);
}

static
void s_test_lat(int echo, int yodel, int nmsgs, size_t msgsize)
{
    zsys_info("test lat: socket types: [%d<->%d], nmsgs=%d msgsize=%ld",
              echo, yodel, msgsize);
    zactor_t *perf_echo = zactor_new (perf_actor, (void*)(size_t)echo);
    assert (perf_echo);
    zactor_t *perf_yodel = zactor_new (perf_actor, (void*)(size_t)yodel);
    assert (perf_yodel);

    char* ep = s_bind(perf_yodel);
    s_conn(perf_echo, ep);
    free (ep);

    s_echo_ini(perf_echo, nmsgs);
    s_yodel(perf_yodel, nmsgs, msgsize);
    s_echo_fin(perf_echo, nmsgs, msgsize);

    zactor_destroy (&perf_echo);
    zactor_destroy (&perf_yodel);

}

static
void s_send_ini(zactor_t* zp, int nmsgs, size_t msgsize)
{
    int rc = zsock_send(zp, "si8", "SEND", nmsgs, msgsize);
    assert(rc == 0);
}

static
void s_recv(zactor_t* zp, int nmsgs, size_t msgsize)
{
    int rc = zsock_send(zp, "si", "RECV", nmsgs);
    assert(rc == 0);

    char* got_cmd=0;
    int got_nmsgs=0, noos=0;
    size_t totdat=0;
    int64_t time_us=0;
    rc = zsock_recv(zp, "si88i", &got_cmd,
                    &got_nmsgs, &totdat, &time_us, &noos);
    assert(rc == 0);
    assert(streq(got_cmd, "RECV"));
    free(got_cmd);
    assert(got_nmsgs == nmsgs);
    assert(totdat == nmsgs*msgsize);
    
    s_report("recv", nmsgs, totdat, time_us);
    zsys_info("recv: %d out-of-order", noos);
}

static
void s_send_fin(zactor_t* zp, int nmsgs, size_t msgsize)
{
    // SEND <nmsgs> <msgsize> <time_us>
    char* got_cmd=0;
    int got_nmsgs=0;
    int64_t time_us=0;
    int rc = zsock_recv(zp, "si88", &got_cmd,
                    &got_nmsgs, &time_us);
    assert(rc == 0);
    assert(streq(got_cmd, "SEND"));
    free(got_cmd);
    assert(got_nmsgs == nmsgs);
    s_report("send", nmsgs, nmsgs*msgsize, time_us);

}

static
void s_test_thr(int src, int dst, int nmsgs, size_t msgsize)
{
    zsys_info("test thr: socket types: [%d->%d], nmsgs=%d msgsize=%ld",
              src, dst, nmsgs, msgsize);

    zactor_t* perf_dst = zactor_new (perf_actor, (void*)(size_t)dst);
    zactor_t* perf_src = zactor_new (perf_actor, (void*)(size_t)src);

    char* ep = s_bind(perf_dst);
    s_conn(perf_src, ep);
    free (ep);

    s_send_ini(perf_src, nmsgs, msgsize);
    s_recv(perf_dst, nmsgs, msgsize);
    s_send_fin(perf_src, nmsgs, msgsize);

    zactor_destroy (&perf_src);
    zactor_destroy (&perf_dst);
}


void
perf_test (bool verbose)
{
    zsys_init();
    zsys_info("testing perf: ");
    //  @selftest

    s_test_lat(ZMQ_REP, ZMQ_REQ, 10000, 1<<10);
    s_test_lat(ZMQ_REP, ZMQ_REQ, 10000, 1<<16);
    s_test_lat(ZMQ_ROUTER, ZMQ_REQ, 10000, 1<<10);
    // fixme: this one hangs
    // s_test_lat(ZMQ_REP, ZMQ_DEALER, 10000, 1<<10);

    s_test_thr(ZMQ_PUSH, ZMQ_PULL, 1000000, 1<<10);
    // fixme: this one hangs
    // s_test_thr(ZMQ_PUB, ZMQ_SUB, 10000, 1<<10);

    //  @end

    printf ("OK\n");
}
