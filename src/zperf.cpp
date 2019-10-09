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
    char* last_endpoint;
    int last_nmsgs;
    size_t last_nbytes;
    int last_noos;
    uint64_t last_cpu;
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
        if (self->last_endpoint) {
            free(self->last_endpoint);
        }
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

const char *
zperf_bind (zperf_t *self, const char *address)
{
    if (self->last_endpoint) {
        freen (self->last_endpoint);
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
    self->last_endpoint = ep;
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

uint64_t
zperf_echo (zperf_t *self, int nmsgs)
{
    zperf_echo_ini(self, nmsgs);
    return zperf_echo_fin(self);
}

static
void s_clear_last(zperf_t* self)
{
    self->last_nmsgs = 0;
    self->last_noos = 0;
    self->last_nbytes = 0;
    self->last_cpu = 0;         /* user+system time in microseconds */
}

static
void s_set_last(zperf_t* self, int nmsgs, int noos, size_t nbytes, uint64_t cpu)
{
    self->last_nmsgs = nmsgs;
    self->last_noos = noos;
    self->last_nbytes = nbytes;
    self->last_cpu = cpu;
}

void
zperf_echo_ini (zperf_t *self, int nmsgs)
{
    s_clear_last(self);
    int rc = zsock_send(self->perf, "si", "ECHO", nmsgs);
    assert(rc == 0);
}

uint64_t
zperf_echo_fin (zperf_t *self)
{
    int nmsgs=0;
    int64_t time_us=0;
    uint64_t cpu_us=0;
    char* cmd=0;
    size_t totdat=0;
    int rc = zsock_recv(self->perf, "si888", &cmd,
                        &nmsgs, &totdat, &time_us, &cpu_us);
    assert(rc == 0);
    assert(streq(cmd, "ECHO"));
    free(cmd);
    s_set_last(self, nmsgs, 0, totdat, cpu_us);
    return time_us;
}

uint64_t
zperf_yodel (zperf_t *self, int nmsgs, uint64_t msgsize)
{
    zperf_yodel_ini(self, nmsgs, msgsize);
    return zperf_yodel_fin(self);
}

void
zperf_yodel_ini (zperf_t *self, int nmsgs, uint64_t msgsize)
{
    s_clear_last(self);
    int rc = zsock_send(self->perf, "si8", "YODEL", nmsgs, msgsize);
    assert(rc == 0);
}

uint64_t
zperf_yodel_fin (zperf_t *self)
{
    int nmsgs=0, noos=0;
    int64_t time_us=0, msgsize=0;
    uint64_t cpu_us=0;
    char* cmd=0;
    int rc = zsock_recv(self->perf, "si888i", &cmd,
                        &nmsgs, &msgsize, &time_us, &cpu_us, &noos);
    assert(rc == 0);
    assert(streq("YODEL", cmd));
    free(cmd);
    s_set_last(self, nmsgs, noos, nmsgs*msgsize, cpu_us);
    return time_us;
}

uint64_t
zperf_send (zperf_t *self, int nmsgs, uint64_t msgsize)
{
    zperf_send_ini(self, nmsgs, msgsize);
    return zperf_send_fin(self);
}

void
zperf_send_ini (zperf_t *self, int nmsgs, uint64_t msgsize)
{
    s_clear_last(self);
    int rc = zsock_send(self->perf, "si8", "SEND", nmsgs, msgsize);
    assert(rc == 0);
}

uint64_t
zperf_send_fin (zperf_t *self)
{
    char* cmd=0;
    int nmsgs=0;
    int64_t time_us=0;
    uint64_t cpu_us=0;
    size_t msgsize=0;
    int rc = zsock_recv(self->perf, "si888", &cmd,
                        &nmsgs, &msgsize, &time_us, &cpu_us);
    assert(rc == 0);
    assert(streq(cmd, "SEND"));
    free(cmd);
    s_set_last(self, nmsgs, 0, nmsgs*msgsize, cpu_us);
    return time_us;
}

uint64_t
zperf_recv (zperf_t *self, int nmsgs)
{
    zperf_recv_ini(self, nmsgs);
    return zperf_recv_fin(self);
}

void
zperf_recv_ini (zperf_t *self, int nmsgs)
{
    s_clear_last(self);
    int rc = zsock_send(self->perf, "si", "RECV", nmsgs);
    assert(rc == 0);
}

uint64_t
zperf_recv_fin (zperf_t *self)
{
    char* cmd=0;
    int nmsgs=0, noos=0;
    size_t totdat=0;
    int64_t time_us=0;
    uint64_t cpu_us=0;
    int rc = zsock_recv(self->perf, "si888i", &cmd,
                        &nmsgs, &totdat, &time_us, &cpu_us, &noos);
    assert(rc == 0);
    assert(streq(cmd, "RECV"));
    free(cmd);
    s_set_last(self, nmsgs, noos, totdat, cpu_us);
    return time_us;
}

int
zperf_noos (zperf_t *self)
{
    return self->last_noos;
}

uint64_t
zperf_bytes (zperf_t *self)
{
    return self->last_nbytes;
}

uint64_t
zperf_cpu (zperf_t* self)
{
    return self->last_cpu;
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
void s_report(const char* what, int nmsgs, size_t totdat, int64_t time_us, uint64_t cpu_us)
{
    double time_s = 1e-6*time_us;
    double Gbps = totdat*8e-9/time_s;
    double kHz = 0.001*nmsgs/time_s;
    double lat_us = 1e6*time_s/nmsgs;
    double cpupc = (100.0 * cpu_us) / time_us;

    zsys_info("%s: %d msgs (%.3f Gbps) in %.3fs, %.3f kHz, %.3f us/msg, %.3f %%CPU",
              what, nmsgs, Gbps, time_s, kHz, lat_us, cpupc);
}

static
void s_test_lat(int echo, int yodel, int nmsgs, size_t msgsize)
{
    zsys_info("zperf test lat: socket types: [%d->%d], nmsgs=%d msgsize=%ld",
              echo, yodel, nmsgs, msgsize);

    zperf_t* zpe = zperf_new(echo);
    assert(zpe);
    zperf_t* zpy = zperf_new(yodel);
    assert(zpy);

    const char* ep = zperf_bind(zpy, "tcp://127.0.0.1:*");
    zperf_connect(zpe, ep);

    zperf_echo_ini(zpe, nmsgs);
    int64_t ty = zperf_yodel(zpy, nmsgs, msgsize);
    int64_t te = zperf_echo_fin(zpe);

    uint64_t cy = zperf_cpu(zpy);
    uint64_t ce = zperf_cpu(zpe);

    s_report("echo", nmsgs, zperf_bytes(zpe), te, ce);
    s_report("yodel", nmsgs, zperf_bytes(zpy), ty, cy);

    zperf_destroy (&zpe);
    zperf_destroy (&zpy);
}

    
static
void s_test_thr(int src, int dst, int nmsgs, size_t msgsize)
{
    zsys_info("zperf test thr: socket types: [%d->%d], nmsgs=%d msgsize=%ld",
              src, dst, nmsgs, msgsize);
    zperf_t* zps = zperf_new(src);
    assert(zps);
    zperf_t* zpd = zperf_new(dst);
    assert(zpd);

    const char* ep = zperf_bind(zpd, "tcp://127.0.0.1:*");
    zperf_connect(zps, ep);

    zperf_send_ini(zps, nmsgs, msgsize);
    int64_t td = zperf_recv(zpd, nmsgs);
    int64_t ts = zperf_send_fin(zps);

    uint64_t cd = zperf_cpu(zpd);
    uint64_t cs = zperf_cpu(zps);

    s_report("send", nmsgs, zperf_bytes(zps), ts, cs);
    s_report("recv", nmsgs, zperf_bytes(zpd), td, cd);

    zperf_destroy (&zps);
    zperf_destroy (&zpd);
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
    s_test_lat(ZMQ_REP, ZMQ_REQ, nmsgs, 1<<10);
    s_test_lat(ZMQ_REP, ZMQ_REQ, nmsgs, 1<<16);
    s_test_lat(ZMQ_ROUTER, ZMQ_REQ, nmsgs, 1<<10);
    // fixme: this one hangs
    // s_test_lat(ZMQ_REP, ZMQ_DEALER, 10000, 1<<10);

    s_test_thr(ZMQ_PUSH, ZMQ_PULL, nmsgs, 1<<10);
    //  @end
    printf ("OK\n");
}
