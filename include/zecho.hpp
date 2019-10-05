/*  =========================================================================
    zecho - A message echo service

    GPL 3.0
    =========================================================================
*/

#ifndef ZECHO_H_INCLUDED
#define ZECHO_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


//  @interface
//  Create new zecho actor instance.
//  @TODO: Describe the purpose of this actor!
//
//      zactor_t *zecho = zactor_new (zecho, NULL);
//
//  Destroy zecho instance.
//
//      zactor_destroy (&zecho);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (zecho, "VERBOSE");
//
//  Start zecho actor.
//
//      zstr_sendx (zecho, "START", NULL);
//
//  Stop zecho actor.
//
//      zstr_sendx (zecho, "STOP", NULL);
//
//  This is the zecho constructor as a zactor_fn;
ZPERFMQ_EXPORT void
    zecho_actor (zsock_t *pipe, void *args);

//  Self test of this actor
ZPERFMQ_EXPORT void
    zecho_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
