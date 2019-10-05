/*  =========================================================================
    zplat - A round-trip latency measure

    GPL 3.0
    =========================================================================
*/

#ifndef ZPLAT_H_INCLUDED
#define ZPLAT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


//  @interface
//  Create new zplat actor instance.
//  @TODO: Describe the purpose of this actor!
//
//      zactor_t *zplat = zactor_new (zplat, NULL);
//
//  Destroy zplat instance.
//
//      zactor_destroy (&zplat);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (zplat, "VERBOSE");
//
//  Start zplat actor.
//
//      zstr_sendx (zplat, "START", NULL);
//
//  Stop zplat actor.
//
//      zstr_sendx (zplat, "STOP", NULL);
//
//  This is the zplat constructor as a zactor_fn;
ZPERFMQ_EXPORT void
    zplat_actor (zsock_t *pipe, void *args);

//  Self test of this actor
ZPERFMQ_EXPORT void
    zplat_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
