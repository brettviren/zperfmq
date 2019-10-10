/*  =========================================================================
    zperf_server - Zperf Server (in C)

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: zperf_server.xml, or
     * The code generation script that built this file: zproto_server_c
    ************************************************************************
    LGPL 3.0
    =========================================================================
*/

#ifndef ZPERF_SERVER_H_INCLUDED
#define ZPERF_SERVER_H_INCLUDED

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  To work with zperf_server, use the CZMQ zactor API:
//
//  Create new zperf_server instance, passing logging prefix:
//
//      zactor_t *zperf_server = zactor_new (zperf_server, "myname");
//
//  Destroy zperf_server instance
//
//      zactor_destroy (&zperf_server);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (zperf_server, "VERBOSE");
//
//  Bind zperf_server to specified endpoint. TCP endpoints may specify
//  the port number as "*" to acquire an ephemeral port:
//
//      zstr_sendx (zperf_server, "BIND", endpoint, NULL);
//
//  Return assigned port number, specifically when BIND was done using an
//  an ephemeral port:
//
//      zstr_sendx (zperf_server, "PORT", NULL);
//      char *command, *port_str;
//      zstr_recvx (zperf_server, &command, &port_str, NULL);
//      assert (streq (command, "PORT"));
//
//  Specify configuration file to load, overwriting any previous loaded
//  configuration file or options:
//
//      zstr_sendx (zperf_server, "LOAD", filename, NULL);
//
//  Set configuration path value:
//
//      zstr_sendx (zperf_server, "SET", path, value, NULL);
//
//  Save configuration data to config file on disk:
//
//      zstr_sendx (zperf_server, "SAVE", filename, NULL);
//
//  Send zmsg_t instance to zperf_server:
//
//      zactor_send (zperf_server, &msg);
//
//  Receive zmsg_t instance from zperf_server:
//
//      zmsg_t *msg = zactor_recv (zperf_server);
//
//  This is the zperf_server constructor as a zactor_fn:
//
void
    zperf_server (zsock_t *pipe, void *args);

//  Self test of this class
void
    zperf_server_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
