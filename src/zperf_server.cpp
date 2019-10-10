/*  =========================================================================
    zperf_server - class description

    LGPL 3.0
    =========================================================================
*/

/*
@header
    zperf_server -
@discuss
@end
*/

#include "zperfmq_classes.hpp"

//  Structure of our class

struct _zperf_server_t {
    int filler;     //  Declare class properties here
};


//  --------------------------------------------------------------------------
//  Create a new zperf_server

zperf_server_t *
zperf_server_new (void)
{
    zperf_server_t *self = (zperf_server_t *) zmalloc (sizeof (zperf_server_t));
    assert (self);
    //  Initialize class properties here
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zperf_server

void
zperf_server_destroy (zperf_server_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zperf_server_t *self = *self_p;
        //  Free class properties here
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
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

void
zperf_server_test (bool verbose)
{
    printf (" * zperf_server: ");

    //  @selftest
    //  Simple create/destroy test
    zperf_server_t *self = zperf_server_new ();
    assert (self);
    zperf_server_destroy (&self);
    //  @end
    printf ("OK\n");
}
