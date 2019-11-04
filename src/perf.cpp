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

#include "zperfmq_classes.hpp"
#include <sys/time.h>
#include <sys/resource.h>
#include "zperf_util.hpp"

//  Structure of our actor

struct _perf_t {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?

    //  Declare properties
    zsock_t* sock;              /* The measurement socket */


    // values active during a measure
    const char* measure;
    int nmsgs;
    int noos;
    size_t msgsize;
    size_t totdata;
    int64_t time_us;
    int64_t cpu_us;

    void* watch;
    uint64_t cpu_start;
    // for eg, ROUTER
    zframe_t* idframe;
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


// Set internal batch/buffer size.  NOTE: this typically should NOT be
// called.  Changing the default may HARM latency/throughput.

static int
perf_set_batch_size(perf_t* self, int size)
{
    assert(self);

    // czmq doesn't yet have the needed method, so operate on zmq socket
    void* s = zsock_resolve(self->sock);
    assert(s);
    int rc=0;

    rc = zmq_setsockopt(s, ZMQ_IN_BATCH_SIZE, &size, sizeof(size));
    if (rc != 0) { return -1; }
    rc = zmq_setsockopt(s, ZMQ_OUT_BATCH_SIZE, &size, sizeof(size));
    if (rc != 0) { return -1; }
    
    return 0;
}



// Bind the measurement socket.

static int
perf_bind (perf_t *self, const char* endpoint)
{
    assert (self);

    int rc=0, port = zsock_bind(self->sock, "%s", endpoint);
    if (self->verbose) {
        zsys_debug("perf: BIND %s %d", endpoint, port);
    }
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
    if (self->verbose) {
        zsys_debug("perf: CONNECT %s %d", endpoint, rc);
    }
    zsock_send(self->pipe, "ssi", "CONNECT", endpoint, rc);
    return rc;
}

static uint64_t s_cpu_now()
{
    struct rusage u;
    getrusage(RUSAGE_SELF, &u);

    return u.ru_utime.tv_usec + u.ru_stime.tv_usec +
        (u.ru_utime.tv_sec + u.ru_stime.tv_sec)*1000000;
}

static void
s_reset(perf_t* self, const char* measure, int nmsgs, size_t msgsize)
{
    if (self->verbose) {
        zsys_debug("perf: start: %s %d %ld", measure, nmsgs, msgsize);
    }

    assert (self);
    self->measure = measure;
    self->nmsgs = nmsgs;
    self->noos = 0;
    self->msgsize = msgsize;
    self->totdata = 0;
    self->time_us = self->cpu_us = 0;
    self->watch = NULL;
    self->cpu_start = 0;
    assert (!self->idframe);
}

static void
s_start(perf_t* self)
{
    self->watch = zmq_stopwatch_start();
    self->cpu_start = s_cpu_now();
}

// Run an echo service.  Return results down pipe.

static
void s_send(perf_t* self, int count, zframe_t* frame)
{    
    // zsys_debug("send: %s count %d, frame %ld",
    //            self->measure, count, zframe_size(frame));

    zmsg_t* msg = zmsg_new();

    if (ZMQ_ROUTER == zsock_type(self->sock)) {
        // zsys_debug("send: %s id frame size %ld",
        //            self->measure, zframe_size(self->idframe));
        zmsg_pushmem(msg, NULL, 0); // delimiter
        zmsg_prepend(msg, &self->idframe);
    }
    if (ZMQ_DEALER == zsock_type(self->sock)) {
        zmsg_pushmem(msg, NULL, 0);
    }

    zmsg_addmem(msg, &count, sizeof(int));
    zframe_t* dup = zframe_dup(frame);
    zmsg_append(msg, &dup);

    zmsg_send(&msg, self->sock);

    // zsys_debug("send: %s count %d and frame of size %ld",
    //            self->measure, count, zframe_size(frame));
}

static
zframe_t* s_recv(perf_t* self, int count)
{    
    // zsys_debug("recv: %s count %d", self->measure, count);

    zmsg_t* msg = zmsg_recv(self->sock);

    if (ZMQ_ROUTER == zsock_type(self->sock)) {
        self->idframe = zmsg_pop(msg);
        zframe_t *delimiter = zmsg_pop(msg);
        zframe_destroy(&delimiter);
    }
    if (ZMQ_DEALER == zsock_type(self->sock)) {
        zframe_t *delimiter = zmsg_pop(msg);
        zframe_destroy(&delimiter);
    }
    
    zframe_t* cframe = zmsg_pop(msg);
    assert(4 == zframe_size(cframe));
    int got_count = *(int*)zframe_data(cframe);
    zframe_destroy(&cframe);

    zframe_t* pframe = zmsg_pop(msg);
    zmsg_destroy(&msg);

    // zsys_debug("recv: %s count %d, frame %ld",
    //            self->measure, got_count, zframe_size(pframe));

    if (count != got_count) {
        ++ self->noos;
    }

    const size_t this_size = zframe_size (pframe);
    if (this_size != self->msgsize) {
        zsys_warning("%s: recv: size mismatch on %d. want: %ld, got: %ld",
                     self->measure, count, self->msgsize, this_size);
        zframe_destroy(&pframe);
        return NULL;
    }
    return pframe;
}

static
int s_signal(perf_t* self)
{
    self->time_us = zmq_stopwatch_stop(self->watch);
    self->cpu_us = s_cpu_now() - self->cpu_start;

    // zsys_debug("%s signal: %d msgs totdata %ld in %ld us and %d ooo",
    //            self->measure, self->nmsgs, self->totdata, self->time_us, self->noos);

    // <mesaure> <nmsgs> <total_size> <time_us> <cpu_us> <noos>
    int rc = zsock_send(self->pipe, "si888i", self->measure,
                        self->nmsgs, self->totdata,
                        self->time_us, self->cpu_us, self->noos);
    if (self->verbose) {
        zsys_debug("perf: done: %s %d %ld %ld",
                   self->measure, self->nmsgs, self->msgsize,
                   self->totdata);
    }
    return rc;
}


zframe_t* s_frame_new(perf_t* self)
{
    zframe_t* frame = zframe_new(NULL, self->msgsize);
    assert(frame);
    memset (zframe_data(frame), 0, zframe_size(frame));
    return frame;
}


static int
perf_echo (perf_t *self, int nmsgs, size_t msgsize)
{
    s_reset(self, "ECHO", nmsgs, msgsize);

    for (int count=0; count<nmsgs; ++count) {

        if (count == 1) {       // start after first yodel received
            s_start(self);      // ignores async starts
        } 

        zframe_t* frame = s_recv(self, count);
        if (!frame) { return -1; }

        self->totdata += zframe_size(frame);

        s_send(self, count, frame);
        zframe_destroy(&frame);
    }
    return s_signal(self);
}

// Run a latency measurement against an echo service.  Return results
// down pipe.

static int
perf_yodel (perf_t *self, int nmsgs, size_t msgsize)
{
    s_reset(self, "YODEL", nmsgs, msgsize);
    zframe_t* frame = s_frame_new(self);
    s_start(self);
    for (int count=0; count < nmsgs; ++count) {
        s_send(self, count, frame);

        zframe_t* got_frame = s_recv(self, count);
        if (!got_frame) { return -1; }
        self->totdata += zframe_size(got_frame);

        zframe_destroy(&got_frame);
    }
    zframe_destroy(&frame);
    return s_signal(self);
}


// Send some data.  Return results down pipe.

static int
perf_send (perf_t *self, int nmsgs, size_t msgsize)
{
    s_reset(self, "SEND", nmsgs, msgsize);
    zframe_t* frame = s_frame_new(self);
    s_start(self);
    for (int count = 0; count<nmsgs; ++count) {
        s_send(self, count, frame);
        self->totdata += zframe_size(frame);
    }
    zframe_destroy(&frame);
    return s_signal(self);
}


// Receive some data.  Return results down pipe.

static int
perf_recv (perf_t *self, int nmsgs, size_t msgsize)
{
    s_reset (self, "RECV", nmsgs, msgsize);
    for (int count = 0; count < nmsgs; ++count) {
        if (count == 1) {           /* start after first message */
            s_start(self);
        }
        zframe_t* frame = s_recv(self, count);
        if (!frame) { return -1; }
        self->totdata += zframe_size(frame);
        zframe_destroy(&frame);        
    }
    return s_signal(self);
}

// the "echo" in libzmq
static int
perf_slat (perf_t* self, int nmsgs, size_t msgsize)
{
    s_reset (self, "SLAT", nmsgs, msgsize);
    void* s = zsock_resolve(self->sock);

    int rc = 0;
    zmq_msg_t msg;

    rc = zmq_msg_init(&msg);
    assert (rc == 0);

    s_start(self);
    for (int count = 0; count < nmsgs; ++count) {
        if (count == 1) {           /* start after first message */
            s_start(self);
        }

        // count
        rc = zmq_msg_recv(&msg, s, 0);
        if (rc != sizeof(int)) {
            zsys_error("perf: error in slat count frame: %s", zmq_strerror(errno));
        }
        assert(rc == sizeof(int));
        int got_count = *(int*)zmq_msg_data(&msg);
        if (count != got_count) {
            ++self->noos;
        }

        // payload
        rc = zmq_recvmsg (s, &msg, 0);
        assert (rc == msgsize);
        self->totdata += msgsize;

        zmq_msg_t cmsg;
        zmq_msg_init_size(&cmsg, sizeof(int));
        *(int*)zmq_msg_data(&cmsg) = got_count;

        rc = zmq_msg_send(&cmsg, s, ZMQ_SNDMORE);
        if (rc != sizeof(int)) {
            zsys_error("perf: error in slat count frame: %s", zmq_strerror(errno));
        }
        assert(rc == sizeof(int));

        rc = zmq_msg_send(&msg, s, 0);
        assert(rc = msgsize);

    }
    return s_signal(self);
}

// the "yodel" in libzmq
static int
perf_rlat (perf_t* self, int nmsgs, size_t msgsize)
{
    s_reset (self, "RLAT", nmsgs, msgsize);
    void* s = zsock_resolve(self->sock);

    int rc = 0;
    zmq_msg_t pmsg;

    rc = zmq_msg_init_size(&pmsg, msgsize);
    assert (rc == 0);

    s_start(self);

    for (int count = 0; count < nmsgs; ++count) {
        zmq_msg_t cmsg;
        zmq_msg_init_size(&cmsg, sizeof(int));
        *(int*)zmq_msg_data(&cmsg) = count;

        rc = zmq_msg_send(&cmsg, s, ZMQ_SNDMORE);
        if (rc != sizeof(int)) {
            zsys_error("perf: error in rlat count frame: %s", zmq_strerror(errno));
        }
        assert(rc == sizeof(int));

        zmq_msg_t pmsg1;
        rc = zmq_msg_init(&pmsg1);
        assert (rc == 0);
        rc = zmq_msg_copy(&pmsg1, &pmsg);

        rc = zmq_msg_send(&pmsg1, s, 0);
        assert(rc = msgsize);

        self->totdata += msgsize;

        // count
        rc = zmq_msg_recv(&cmsg, s, 0);
        if (rc != sizeof(int)) {
            zsys_error("perf: error in rlat count frame: %s", zmq_strerror(errno));
        }
        assert(rc == sizeof(int));
        int got_count = *(int*)zmq_msg_data(&cmsg);
        if (count != got_count) {
            ++self->noos;
            // if (self->verbose) {
            //     zsys_debug("perf: %d oos, want: %d, got: %d", self->noos, count, got_count);
            // }

        }

        // payload
        rc = zmq_recvmsg (s, &pmsg1, 0);
        assert (rc == msgsize);

    }
    return s_signal(self);
}


static int
perf_sthr (perf_t* self, int nmsgs, size_t msgsize)
{
    s_reset (self, "STHR", nmsgs, msgsize);
    void* s = zsock_resolve(self->sock);

    int rc=0;
    zmq_msg_t pmsg;

    rc = zmq_msg_init_size(&pmsg, msgsize);
    assert (rc == 0);

    s_start(self);
    for (int count = 0; count < nmsgs; ++count) {
        zmq_msg_t cmsg;
        zmq_msg_init_size(&cmsg, sizeof(int));
        *(int*)zmq_msg_data(&cmsg) = count;

        rc = zmq_msg_send(&cmsg, s, ZMQ_SNDMORE);
        if (rc != sizeof(int)) {
            zsys_error("perf: error in send count frame: %s", zmq_strerror(errno));
        }
        assert(rc == sizeof(int));

        zmq_msg_t pmsg1;
        rc = zmq_msg_init(&pmsg1);
        assert (rc == 0);
        rc = zmq_msg_copy(&pmsg1, &pmsg);

        rc = zmq_msg_send(&pmsg1, s, 0);
        assert(rc = msgsize);
    }
    rc = zmq_msg_close(&pmsg);
    assert(rc == 0);


    self->totdata = nmsgs*msgsize;
    return s_signal(self);
}


static int
perf_rthr (perf_t* self, int nmsgs, size_t msgsize)
{
    s_reset (self, "RTHR", nmsgs, msgsize);
    void* s = zsock_resolve(self->sock);

    int rc=0;
    zmq_msg_t msg;
    rc = zmq_msg_init(&msg);
    assert(rc == 0);
    
    for (int count = 0; count < nmsgs; ++count) {
        if (count == 1) {           /* start after first message */
            s_start(self);
        }

        // count
        rc = zmq_msg_recv(&msg, s, 0);
        if (rc != sizeof(int)) {
            zsys_error("perf: error in recv count frame: %s", zmq_strerror(errno));
        }
        assert(rc == sizeof(int));
        int got_count = *(int*)zmq_msg_data(&msg);
        if (count != got_count) {
            ++self->noos;
        }

        // payload
        rc = zmq_recvmsg (s, &msg, 0);
        assert (rc == msgsize);
    }
    zmq_msg_close(&msg);
    self->totdata = nmsgs*msgsize;
    return s_signal(self);
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

    int rc = 0;
    // BATCH <bytes>
    if (streq (command, "BATCH")) {
        int size = pop_int (request);        
        rc = perf_set_batch_size(self, size);
    }
    // BIND <address> ->
    // BIND <fq-address> <port#, zero or err>
    else if (streq (command, "BIND")) {
        char *endpoint = zmsg_popstr (request);        
        rc = perf_bind (self, endpoint);
        if (rc > 0) { rc = 0; }
        free (endpoint);        
    }
    // CONNECT <address> ->
    // CONNECT <address> <rc>
    else if (streq (command, "CONNECT")) {
        char *endpoint = zmsg_popstr (request);
        rc = perf_connect (self, endpoint);
        free (endpoint);
    }

    // the four measurements of the apocalypse

    // ECHO <nmsgs> <msgsize> ->
    // ECHO <nmsgs> <total_size> <time_us> <cpu_us> <noos>
    else if (streq (command, "ECHO")) {
        const int nmsgs = pop_int(request);
        const size_t msgsize = pop_long(request);
        rc = perf_echo (self, nmsgs, msgsize);
    }
    // YODEL <nmsgs> <msgsize> ->
    // YODEL <nmsgs> <total_size> <time_us> <cpu_us> <noos>
    else if (streq (command, "YODEL")) {
        const int nmsgs = pop_int(request);
        const size_t msgsize = pop_long(request);
        rc = perf_yodel (self, nmsgs, msgsize);
    }
    // SEND <nmsgs> <msgsize> ->
    // SEND <nmsgs> <total_size> <time_us> <cpu_us> <noos>
    else if (streq (command, "SEND")) {
        const int nmsgs = pop_int(request);
        const size_t msgsize = pop_long(request);
        rc = perf_send (self, nmsgs, msgsize);
    }
    // RECV <nmsgs> <msgsize> ->
    // RECV <nmsgs> <total_size> <time_us> <cpu_us> <noos>
    else if (streq (command, "RECV")) {
        const int nmsgs = pop_int(request);
        const size_t msgsize = pop_long(request);
        rc = perf_recv (self, nmsgs, msgsize);
    }

    // libzmq versions

    else if (streq (command, "SLAT")) {
        const int nmsgs = pop_int(request);
        const size_t msgsize = pop_long(request);
        rc = perf_slat (self, nmsgs, msgsize);
    }
    else if (streq (command, "RLAT")) {
        const int nmsgs = pop_int(request);
        const size_t msgsize = pop_long(request);
        rc = perf_rlat (self, nmsgs, msgsize);
    }
    else if (streq (command, "STHR")) {
        const int nmsgs = pop_int(request);
        const size_t msgsize = pop_long(request);
        rc = perf_sthr (self, nmsgs, msgsize);
    }
    else if (streq (command, "RTHR")) {
        const int nmsgs = pop_int(request);
        const size_t msgsize = pop_long(request);
        rc = perf_rthr (self, nmsgs, msgsize);
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

    if (rc != 0) {              // pretend to handle errors
        zsys_error("perf: error (%d) from command %s: %s",
                   rc, command, zmq_strerror(errno));
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
void s_report(const char* what, int nmsgs, size_t totdat, int64_t time_us, uint64_t cpu_us, int noos)
{
    double time_s = 1e-6*time_us;
    double Gbps = totdat*8e-9/time_s;
    double kHz = 0.001*nmsgs/time_s;
    double lat_us = 1e6*time_s/nmsgs;
    double cpupc = (100.0 * cpu_us) / time_us;
    zsys_info("%s: %d msgs (%.3f Gbps) in %.3fs, %.3f kHz, %.3f us/msg, %.3f %%CPU, %d ooo",
              what, nmsgs, Gbps, time_s, kHz, lat_us, cpupc, noos);
    assert(noos == 0);          // will fail with multiple I/O threads
}

static
void s_ini(zactor_t* zp, const char* measure, int nmsgs, size_t msgsize)
{
    int rc = zsock_send(zp, "si8", measure, nmsgs, msgsize);
    assert(rc == 0);
}

static
void s_fin(zactor_t* zp, const char* measure, int nmsgs, size_t msgsize)
{
    int got_nmsgs=0;
    int64_t got_time=0;
    uint64_t got_cpu=0;
    char* got_cmd=0;
    size_t got_size=0;
    int got_noos = 0;
    int rc = zsock_recv(zp, "si888i", &got_cmd,
                        &got_nmsgs, &got_size, &got_time, &got_cpu, &got_noos);
    assert(rc == 0);
    assert(streq(got_cmd, measure));
    free(got_cmd);
    if (nmsgs != got_nmsgs) {
        zsys_warning("%s: fin: message count mismatch: want:%d got:%d",
                     measure, nmsgs, got_nmsgs);
    }
    assert(nmsgs == got_nmsgs);
    size_t want_size = nmsgs*msgsize;
    if (want_size != got_size) {
        zsys_warning("%s: fin: data size mismatch: want:%ld got:%ld",
                     measure, want_size, got_size);
    }
    
    assert(want_size == got_size);
    s_report(measure, got_nmsgs, got_size, got_time, got_cpu, got_noos);
}

static
void s_bookends(const char* title,
                const char* pitcher_name, int pitcher_socket, // connects
                const char* catcher_name, int catcher_socket, // binds
                int nmsgs, size_t msgsize, bool reverse,      // b<-->c
                bool verbose)
{
    if (verbose) {
        zsys_info("perf test %s: %s[%d]<-->%s[%d] %d of %ld",
                  title,
                  pitcher_name, pitcher_socket,
                  catcher_name, catcher_socket,
                  nmsgs, msgsize);
    }

    zactor_t *p = zactor_new (perf_actor, (void*)(size_t)pitcher_socket);
    assert (p);
    zactor_t *c = zactor_new (perf_actor, (void*)(size_t)catcher_socket);
    assert (c);
    
    if (verbose) {
        int rc = 0;
        rc = zsock_send(p, "s", "VERBOSE");
        assert (rc == 0);
        rc = zsock_send(c, "s", "VERBOSE");
        assert (rc == 0);
    }

    if (reverse) {
        zactor_t* temp = p;
        p = c;
        c = temp;
    }

    char* ep = s_bind(c);
    s_conn(p, ep);
    free (ep);

    s_ini(p, pitcher_name, nmsgs, msgsize);
    s_ini(c, catcher_name, nmsgs, msgsize);
    s_fin(c, catcher_name, nmsgs, msgsize);
    s_fin(p, pitcher_name, nmsgs, msgsize);

    zactor_destroy (&p);
    zactor_destroy (&c);
}


void
perf_test (bool verbose)
{
    zsys_init();
    zsys_info("testing perf: ");
    //  @selftest

    // Keep these tests well under 1 second each so when they are run
    // with valgrind they do not take forever.  10k for lat takes
    // about 15-30s, 2-3 s for thr.
    int nmsgs = 10000;

    s_bookends("lat", "YODEL", ZMQ_REQ, "ECHO", ZMQ_REP, nmsgs, 1<<10, false, verbose);
    s_bookends("lat", "YODEL", ZMQ_REQ, "ECHO", ZMQ_REP, nmsgs, 1<<16, false, verbose);
    s_bookends("lat", "YODEL", ZMQ_REQ, "ECHO", ZMQ_ROUTER, nmsgs, 1<<10, false, verbose);
    s_bookends("lat", "YODEL", ZMQ_DEALER, "ECHO", ZMQ_REP, nmsgs, 1<<10, false, verbose);

    s_bookends("lat", "RLAT", ZMQ_REQ,  "SLAT", ZMQ_REP, nmsgs, 1<<10, false, verbose);
    /// not yet supported
    // s_bookends("lat", "RLAT", ZMQ_REQ,  "SLAT", ZMQ_ROUTER, nmsgs, 1<<10, false, verbose);
    // s_bookends("lat", "RLAT", ZMQ_DEALER,  "SLAT", ZMQ_REP, nmsgs, 1<<10, false, verbose);

    s_bookends("thr", "SEND", ZMQ_PUSH, "RECV", ZMQ_PULL, nmsgs, 1<<10, false, verbose);
    s_bookends("thr", "STHR", ZMQ_PUSH, "RTHR", ZMQ_PULL, nmsgs, 1<<10, false, verbose);

    //  @end

    printf ("OK\n");
}
