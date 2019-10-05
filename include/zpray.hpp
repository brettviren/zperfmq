/*  =========================================================================
    zpray - A message spray service

    GPL 3.0
    =========================================================================
*/

#ifndef ZPRAY_H_INCLUDED
#define ZPRAY_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


//  @interface
//  Create new zpray actor instance.
//  @TODO: Describe the purpose of this actor!
//
//      zactor_t *zpray = zactor_new (zpray, NULL);
//
//  Destroy zpray instance.
//
//      zactor_destroy (&zpray);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (zpray, "VERBOSE");
//
//  Start zpray actor.
//
//      zstr_sendx (zpray, "START", NULL);
//
//  Stop zpray actor.
//
//      zstr_sendx (zpray, "STOP", NULL);
//
//  This is the zpray constructor as a zactor_fn;
ZPERFMQ_EXPORT void
    zpray_actor (zsock_t *pipe, void *args);

//  Self test of this actor
ZPERFMQ_EXPORT void
    zpray_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
