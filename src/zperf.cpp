/*  =========================================================================
    zperf - API to the zperfmq perf actor.

    LGPL 3.0
    =========================================================================
*/

/*
@header
    zperf - API to the zperfmq perf actor.
@discuss
@end
*/

#include "zperfmq_classes.hpp"

//  Structure of our class

struct _zperf_t {
    zactor_t* perf;

    char* endpoint;
    char* name;
    int nmsgs;
    size_t nbytes;
    int noos;
    uint64_t time, cpu, msgsize;
};


//  --------------------------------------------------------------------------
//  Create a new zperf

zperf_t *
zperf_new (int socket_type)
{
    // // inject niothreads here?
    // zmq_init(1);

    zperf_t *self = (zperf_t *) zmalloc (sizeof (zperf_t));
    assert (self);
    //  Initialize class properties here

    self->perf = zactor_new (perf_actor, (void*)(size_t)socket_type);

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zperf

void
zperf_destroy (zperf_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zperf_t *self = *self_p;
        //  Free class properties here
        zactor_destroy(&self->perf);
        if (self->endpoint) {
            free(self->endpoint);
        }
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

const char *
zperf_bind (zperf_t *self, const char *address)
{
    if (self->endpoint) {
        freen (self->endpoint);
    }
    int rc = zsock_send(self->perf, "ss", "BIND", address);
    if (rc != 0) {
        return NULL;
    }
    char* ep=0;
    char* cmd=0;
    int port=-1;
    rc = zsock_recv(self->perf, "ssi", &cmd, &ep, &port);
    assert(streq(cmd, "BIND"));
    free(cmd);
    self->endpoint = ep;
    if (rc !=0 || port < 0 || !ep) {
        return NULL;
    }
    return ep;
}


int
zperf_connect (zperf_t *self, const char *address)
{
    int rc = zsock_send(self->perf, "ss", "CONNECT", address);
    if (rc != 0) {
        return rc;
    }
    char* cmd = 0;
    char* ep = 0;
    int rc2 = zsock_recv(self->perf, "ssi", &cmd, &ep, &rc);
    assert(streq("CONNECT", cmd));
    free(cmd);
    assert(streq(ep, address));
    free(ep);
    if (rc != 0) {
        return rc;
    }
    return rc2;
}

static
void s_clear_last(zperf_t* self)
{
    if (self->name) {
        free (self->name);
        self->name = 0;
    }
    self->nmsgs = 0;
    self->msgsize = 0;
    self->noos = 0;
    self->nbytes = 0;
    self->time = 0;
    self->cpu = 0;         /* user+system time in microseconds */
}

static
void s_set_last(zperf_t* self, char* name, int nmsgs, int noos, size_t nbytes,
                uint64_t time, uint64_t cpu)
{
    if (self->name) {
        free (self->name);
    }
    self->name = name;
    self->nmsgs = nmsgs;
    self->noos = noos;
    self->nbytes = nbytes;
    self->time = time;    
    self->cpu = cpu;
}

uint64_t
zperf_measure(zperf_t* self, const char *name, int nmsgs, uint64_t msgsize)
{
    zperf_initiate(self, name, nmsgs, msgsize);
    return zperf_finalize(self);
}

void
zperf_initiate(zperf_t* self, const char *name, int nmsgs, uint64_t msgsize)
{
    s_clear_last(self);
    self->msgsize = msgsize;
    int rc = zsock_send(self->perf, "si8", name, nmsgs, msgsize);
    assert(rc == 0);
}

uint64_t
zperf_finalize (zperf_t *self)
{
    int nmsgs=0, noos=0;
    uint64_t time_us=0, cpu_us=0, totdat=0;
    char* name=0;

    int rc = zsock_recv(self->perf, "si888i", &name,
                        &nmsgs, &totdat, &time_us, &cpu_us, &noos);

    assert(rc == 0);

    s_set_last(self, name, nmsgs, noos, totdat, time_us, cpu_us);

    return time_us;
}
int
zperf_nmsgs (zperf_t *self)
{
    return self->nmsgs;
}

uint64_t
zperf_msgsize (zperf_t *self)
{
    return self->msgsize;
}

const char*
zperf_name (zperf_t* self)
{
    return self->name;
}

int
zperf_noos (zperf_t *self)
{
    return self->noos;
}

uint64_t
zperf_bytes (zperf_t *self)
{
    return self->nbytes;
}

uint64_t
zperf_time (zperf_t* self)
{
    return self->time;
}

uint64_t
zperf_cpu (zperf_t* self)
{
    return self->cpu;
}


//  --------------------------------------------------------------------------
//  Self test of this class

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
void s_report(zperf_t* zp)
{
    const char* name = zperf_name(zp);
    uint64_t time_us = zperf_time(zp);
    int nmsgs = zperf_nmsgs(zp);
    uint64_t nbytes = zperf_bytes(zp);

    double time_s = 1e-6*time_us;
    double Gbps = nbytes*8e-9/time_s;
    double kHz = 0.001*nmsgs/time_s;
    double lat_us = 1e6*time_s/nmsgs;
    double cpupc = (100.0 * zperf_cpu(zp)) / time_us;

    zsys_info("%s: %d msgs (%.3f Gbps) in %.3fs, %.3f kHz, %.3f us/msg, %.3f %%CPU",
              name, nmsgs, Gbps, time_s, kHz, lat_us, cpupc);
}

static
void s_test(const char* title,
            const char* pitcher_name, int pitcher_socket, // connects
            const char* catcher_name, int catcher_socket, // binds
            int nmsgs, size_t msgsize, bool reverse)      // b<-->c
{
    zsys_info("zperf test %s: %s[%d]<-->%s[%d] %d of %ld",
              title,
              pitcher_name, pitcher_socket,
              catcher_name, catcher_socket,
              nmsgs, msgsize);

    zperf_t* zpp = zperf_new(pitcher_socket);
    assert(zpp);
    zperf_t* zpc = zperf_new(catcher_socket);
    assert(zpc);

    if (reverse) {
        const char* ep = zperf_bind(zpp, "tcp://127.0.0.1:*");
        zperf_connect(zpc, ep);
    }
    else {
        const char* ep = zperf_bind(zpc, "tcp://127.0.0.1:*");
        zperf_connect(zpp, ep);
    }

    zperf_initiate(zpp, pitcher_name, nmsgs, msgsize);
    zperf_initiate(zpc, catcher_name, nmsgs, msgsize);
    zperf_finalize(zpp);
    s_report(zpp);
    zperf_finalize(zpc);
    s_report(zpc);

    zperf_destroy(&zpc);
    zperf_destroy(&zpp);

}

void
zperf_test (bool verbose)
{
    zsys_init();
    zsys_info("testing zperf");

    //  @selftest

    // Keep these tests well under 1 second each so when they are run
    // with valgrind they do not take forever.  10k for lat takes
    // about 15-30s, 2-3 s for thr.
    int nmsgs = 10000;
    
    s_test("lat", "YODEL", ZMQ_REQ, "ECHO", ZMQ_REP, nmsgs, 1<<10, false);
    s_test("lat", "YODEL", ZMQ_REQ, "ECHO", ZMQ_REP, nmsgs, 1<<16, false);
    s_test("lat", "YODEL", ZMQ_REQ, "ECHO", ZMQ_ROUTER, nmsgs, 1<<10, false);
    s_test("lat", "YODEL", ZMQ_DEALER, "ECHO", ZMQ_REP, nmsgs, 1<<10, false);

    s_test("thr", "SEND", ZMQ_PUSH, "RECV", ZMQ_PULL, nmsgs, 1<<10, false);

    //  @end
    printf ("OK\n");
}
