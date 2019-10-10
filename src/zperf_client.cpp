/*  =========================================================================
    zperf_client - class description

    LGPL 3.0
    =========================================================================
*/

/*
@header
    zperf_client -
@discuss
@end
*/

#include "zperfmq_classes.hpp"

//  Structure of our class

struct _zperf_client_t {
    int filler;     //  Declare class properties here
};


//  --------------------------------------------------------------------------
//  Create a new zperf_client

zperf_client_t *
zperf_client_new (void)
{
    zperf_client_t *self = (zperf_client_t *) zmalloc (sizeof (zperf_client_t));
    assert (self);
    //  Initialize class properties here
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zperf_client

void
zperf_client_destroy (zperf_client_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zperf_client_t *self = *self_p;
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
zperf_client_test (bool verbose)
{
    printf (" * zperf_client: ");

    //  @selftest
    //  Simple create/destroy test
    zperf_client_t *self = zperf_client_new ();
    assert (self);
    zperf_client_destroy (&self);
    //  @end
    printf ("OK\n");
}
