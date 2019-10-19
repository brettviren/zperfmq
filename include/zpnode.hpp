/*  =========================================================================
    zpnode - An actor managing a Zyre node and a bundle of zperf clients and servers.

    LGPL 3.0
    =========================================================================
*/

#ifndef ZPNODE_H_INCLUDED
#define ZPNODE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


//  @interface
//  Create new zpnode actor instance.
//  @TODO: Describe the purpose of this actor!
//
//      zactor_t *zpnode = zactor_new (zpnode, NULL);
//
//  Destroy zpnode instance.
//
//      zactor_destroy (&zpnode);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (zpnode, "VERBOSE");
//
//  Start zpnode actor.
//
//      zstr_sendx (zpnode, "START", NULL);
//
//  Stop zpnode actor.
//
//      zstr_sendx (zpnode, "STOP", NULL);
//
//  This is the zpnode constructor as a zactor_fn;
ZPERFMQ_EXPORT void
    zpnode_actor (zsock_t *pipe, void *args);

//  Self test of this actor
ZPERFMQ_EXPORT void
    zpnode_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
