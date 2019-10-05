/*  =========================================================================
    zpthr - A throughput measure

    GPL 3.0
    =========================================================================
*/

#ifndef ZPTHR_H_INCLUDED
#define ZPTHR_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


//  @interface
//  Create new zpthr actor instance.
//  @TODO: Describe the purpose of this actor!
//
//      zactor_t *zpthr = zactor_new (zpthr, NULL);
//
//  Destroy zpthr instance.
//
//      zactor_destroy (&zpthr);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (zpthr, "VERBOSE");
//
//  Start zpthr actor.
//
//      zstr_sendx (zpthr, "START", NULL);
//
//  Stop zpthr actor.
//
//      zstr_sendx (zpthr, "STOP", NULL);
//
//  This is the zpthr constructor as a zactor_fn;
ZPERFMQ_EXPORT void
    zpthr_actor (zsock_t *pipe, void *args);

//  Self test of this actor
ZPERFMQ_EXPORT void
    zpthr_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
