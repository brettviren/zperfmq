/*  =========================================================================
    zpnode - An actor managing a Zyre node a zperf client and/or server.

    LGPL 3.0
    =========================================================================
*/

/*
@header
    zpnode - An actor managing a Zyre node a zperf client and/or server.
@discuss
@end
*/

#include "zperfmq_classes.hpp"
#include "zperf_util.hpp"

// Set the Zyre header key names used.  We take a hint about the
// deprecation of "X-" prefix in HTTP header names and don't use it
// here, although other Zyre apps do use the "X-" prefix.
#define ZPERF_SERVER_ENDPOINT "ZPERF-SERVER-ENDPOINT"
#define ZPERF_SERVER_NICKNAME "ZPERF-SERVER-NICKNAME"

//  Structure of our actor

struct _zpnode_t {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?
    //  TODO: Declare properties

    const char* name;
    zyre_t* zyre;
    bool started;

    zhashx_t* connections;      // server name -> zperf_client
    zhashx_t* wanted_connections; // server names we wait to discover

    zactor_t* server;           // we may also run a server.
    char* server_nickname;
    char* server_endpoint;

};


static void
s_connection_destroy(zperf_client_t** self_p)
{
    assert (self_p);
    if (! *self_p) { return;}
    zperf_client_t* self = *self_p;
    zperf_client_destroy(&self);
    *self_p = 0;
}

//  --------------------------------------------------------------------------
//  Create a new zpnode instance

static zpnode_t *
zpnode_new (zsock_t *pipe, void *args)
{
    zpnode_t *self = (zpnode_t *) zmalloc (sizeof (zpnode_t));
    assert (self);

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);

    //  Initialize properties
    self->name = (const char*)args;
    self->zyre = zyre_new(self->name);
    self->server = NULL;

    self->connections = zhashx_new();
    zhashx_set_destructor(self->connections,
                          (czmq_destructor*) s_connection_destroy);
    self->wanted_connections = zhashx_new();
    
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zpnode instance

static void
zpnode_destroy (zpnode_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zpnode_t *self = *self_p;

        //  Free actor properties
        zyre_destroy(&self->zyre);
        if (self->server) {
            zactor_destroy(&self->server);
        }
        // fixme: free values
        zhashx_destroy(&self->connections);
        zhashx_destroy(&self->wanted_connections);
        //  Free object itself
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}


//  Start this actor. Return a value greater or equal to zero if initialization
//  was successful. Otherwise -1.

static int
zpnode_start (zpnode_t *self)
{
    assert (self);
    assert (!self->started);

    if (self->verbose) {
        zsys_debug("%s: starting", self->name);
    }
    if (zyre_start (self->zyre) == 0) {
        zpoller_add (self->poller, zyre_socket (self->zyre));
        self->started = true;
    }

    return 0;
}


//  Stop this actor. Return a value greater or equal to zero if stopping
//  was successful. Otherwise -1.

static int
zpnode_stop (zpnode_t *self)
{
    assert (self);

    //  TODO: Add shutdown actions

    return 0;
}

// Ephemeral bind and do a little dance to construct endpoint.  Caller
// takes endpoint.

static char*
zpnode_bind_and_endpoint(zpnode_t *self)
{
    zstr_sendx (self->server, "BIND", "tcp://*:*", NULL);
    zsock_send (self->server, "s", "PORT");
    int port_nbr=0;
    zsock_recv (self->server, "si", NULL, &port_nbr);    
    zactor_t *beacon = zactor_new (zbeacon, NULL);
    assert (beacon);
    zsock_send (beacon, "si", "CONFIGURE", 31415);
    char *hostname = zstr_recv (beacon);
    char* endpoint = zsys_sprintf ("tcp://%s:%d", hostname, port_nbr);
    zstr_free (&hostname);
    zactor_destroy (&beacon);    
    return endpoint;
}
    

// Want to connect.  Go through all known zyre peers and if found,
// then connect, else save requested servername for later to check
// each time a new zyre peer is discovered.

static int
zpnode_connect_endpoint(zpnode_t* self, zperf_client_t* client,
                        const char* endpoint)
{
    int rc = zperf_client_say_hello(client, self->name, endpoint);
    if (self->verbose) {
        zsys_debug("%s: connect client to %s (%s)",
                   self->name, endpoint,
                   rc == 0 ? "success" : "failure");
    }
    return rc;
}

// After learning about a new server we check wanted servers and if
// found we connect.  Return true if we did a connect.  

static bool
zpnode_maybe_connect(zpnode_t* self, const char* nn, const char* ep)
{
    char* sn = (char*)zhashx_lookup(self->wanted_connections, nn);
    if (!sn) {
        return false;
    }

    zperf_client_t* client = (zperf_client_t*)zhashx_lookup(self->connections, nn);
    if (!client) {
        zsys_warning("%s: no existing client for wanted server %s",
                     self->name, nn);
        return false;
    }

    if (self->verbose) {
        zsys_debug("%s: server %s came online at %s",
                   self->name, nn, ep);
    }
    zpnode_connect_endpoint(self, client, ep);
    zhashx_delete(self->wanted_connections, nn);
    free(sn);
    return true;
}


// Create the server, bind it, add address to zyre

static int
zpnode_server (zpnode_t *self, const char* nickname)
{
    assert (self);
    if (self->server) {
        zsys_error("%s: server already created");
        return -1;
    }

    self->server_nickname = strdup(nickname);
    self->server = zactor_new(zperf_server, (void*)self->server_nickname);
    if (self->verbose) {
        zstr_send (self->server, "VERBOSE");
    }
    zpoller_add(self->poller, zactor_sock(self->server));

    char *endpoint = zpnode_bind_and_endpoint(self);
    self->server_endpoint = endpoint;
    zyre_set_header(self->zyre, ZPERF_SERVER_ENDPOINT, "%s", endpoint);
    zyre_set_header(self->zyre, ZPERF_SERVER_NICKNAME, "%s", nickname);

    if (self->verbose) {
        zsys_debug("%s: made server %s at %s",
                   self->name, nickname, endpoint);
    }

    // zyre does not self-discover, and our client may want to connect
    // to own own server.
    zpnode_maybe_connect(self, nickname, endpoint);



    return 0;
}

// return client associated with server name, creating if needed.
static int
zpnode_connect(zpnode_t* self, const char* servername)
{
    assert (self);

    zperf_client_t* client = (zperf_client_t*)zhashx_lookup(self->connections,
                                                            servername);
    if (client) {
        zsys_warning("%s: already connected to %s", self->name, servername);
        return 0;
    }

    client = zperf_client_new();
    if (self->verbose) {
        zstr_send (client, "VERBOSE");
        zsys_debug("%s: make client for %s", self->name, servername);
    }
    zhashx_insert(self->connections, servername, client);
    zpoller_add(self->poller, zperf_client_msgpipe(client));

    // First check if own server matches because zyre doesn't discover self.
    if (self->server_nickname && streq(self->server_nickname, servername)) {
        if (self->verbose) {
            zsys_debug("%s: server %s inside node at %s",
                       self->name, self->server_nickname,
                       self->server_endpoint);
        }
        return zpnode_connect_endpoint(self, client,
                                       self->server_endpoint);
    }

    zlist_t* peers = zyre_peers(self->zyre);
    int npeers = zlist_size(peers);
    bool found_it = false;
    for (int ind = 0; ind < npeers; ++ind) {
        char * peer_uuid = (char*)zlist_pop(peers);

        char* nn = zyre_peer_header_value (self->zyre,
                                           peer_uuid, ZPERF_SERVER_NICKNAME);
        char* ep = zyre_peer_header_value (self->zyre,
                                           peer_uuid, ZPERF_SERVER_ENDPOINT);
        if (self->verbose) {
            if (nn) zsys_debug("%s: see server nickname %s", self->name, nn);
            if (ep) zsys_debug("%s: see server endpoint %s", self->name, ep);
        }

        if (nn && ep) {
            if (streq(nn, servername)) {
                found_it = true;
                if (self->verbose) {
                    zsys_debug("%s: server %s already online at %s",
                               self->name, nn, ep);
                }
                int rc = zpnode_connect_endpoint(self, client, ep);
                if (rc != 0) { break; }
            }
        }
        if (nn) { free(nn); }
        if (ep) { free(ep); }

        free(peer_uuid);
    }
    zlist_destroy(&peers);    
    if (found_it) {
        return 0;
    }
    if (self->verbose) {
        zsys_debug("%s: client will wait for discovery of %s",
                   self->name, servername);
    }
    zhashx_insert(self->wanted_connections, servername, strdup(servername));
    return 0;
}

static int
zpnode_latency(zpnode_t* self, const char* sn1, const char* sn2,
               int nmsgs, size_t msgsize)
{
    zperf_client_t* c1 = (zperf_client_t*)zhashx_lookup(self->connections, sn1);
    zperf_client_t* c2 = (zperf_client_t*)zhashx_lookup(self->connections, sn2);
    assert (c1 && c2);

    int rc = 0;

    rc = zperf_client_create_perf(c1, ZMQ_REP);
    assert(rc == 0);
    const char* ident1 = zperf_client_ident(c1);
    rc = zperf_client_create_perf(c2, ZMQ_REQ);
    assert(rc == 0);
    const char* ident2 = zperf_client_ident(c2);
    
    rc = zperf_client_launch_measure(c1, ident1, "ECHO", nmsgs, msgsize, 0);
    assert(rc == 0);
    rc = zperf_client_status(c1);
    assert(rc == 0);

    rc = zperf_client_launch_measure(c2, ident2, "YODEL", nmsgs, msgsize, 0);
    assert(rc == 0);
    rc = zperf_client_status(c2);
    assert(rc == 0);

    zmsg_t* msg = NULL;
    msg = zmsg_recv(zperf_client_msgpipe(c1));
    zmsg_send(&msg, self->pipe); 
    msg = zmsg_recv(zperf_client_msgpipe(c2));
    zmsg_send(&msg, self->pipe); 
    
    return 0;
}


//  Here we handle incoming message from the node

static void
zpnode_recv_api (zpnode_t *self)
{
    //  Get the whole message of the pipe in one go
    zmsg_t *request = zmsg_recv (self->pipe);
    if (!request)
       return;        //  Interrupted

    char *command = zmsg_popstr (request);
    if (self->verbose) {
        zsys_debug("%s: command: %s", self->name, command);
    }
    if (streq (command, "START")) {
        zpnode_start (self);
    }
    else if (streq (command, "STOP")) {
        zpnode_stop (self);
    }
    else if (streq (command, "SERVER")) {
        char* nickname = zmsg_popstr(request);
        zpnode_server(self, nickname);
        free(nickname);
    }
    else if (streq (command, "CONNECT")) {
        char* servername = zmsg_popstr(request);
        zpnode_connect(self, servername);
        free(servername);
    }
    else if (streq (command, "LATENCY")) {
        char* sn1 = zmsg_popstr(request);
        char* sn2 = zmsg_popstr(request);
        int nmsgs = pop_int(request);
        size_t msgsize = pop_long(request);
        zpnode_latency(self, sn1, sn2, nmsgs, msgsize);
        free(sn1);
        free(sn2);
    }
    else if (streq (command, "VERBOSE")) {
        self->verbose = true;
        zyre_set_verbose(self->zyre);
    }
    else if (streq (command, "$TERM")) {
        //  The $TERM command is send by zactor_destroy() method
        self->terminated = true;
    }
    else {
        zsys_error ("%s: invalid command '%s'", self->name, command);
        assert (false);
    }
    zstr_free (&command);
    zmsg_destroy (&request);
}


static void
zpnode_handle_zyre (zpnode_t *self)
{
    zyre_event_t *event = zyre_event_new (self->zyre);
    zsys_debug("%s: peer %s, ID %s, event: %s",
               self->name,
               zyre_event_peer_name(event),
               zyre_event_peer_uuid(event),
               zyre_event_type(event));


    if (streq(zyre_event_type (event), "ENTER")) {
        zsys_debug("%s: peer %s, ID %s enter",
                   self->name,
                   zyre_event_peer_name(event),
                   zyre_event_peer_uuid(event));
        const char *ep = zyre_event_header (event, ZPERF_SERVER_ENDPOINT);
        const char *nn = zyre_event_header (event, ZPERF_SERVER_NICKNAME);
        if (ep && nn) {
            zpnode_maybe_connect(self, nn, ep);
        }
    }
    else if (streq(zyre_event_type (event), "EXIT")) {
        zsys_debug("%s: peer %s, ID %s exit",
                   self->name,
                   zyre_event_peer_name(event),
                   zyre_event_peer_uuid(event));
    }
    zyre_event_destroy (&event);
}

static void
s_self_handle_server (zpnode_t *self)
{
    zmsg_t *request = zmsg_recv (zactor_sock(self->server));
    char *command = zmsg_popstr (request);
    if (self->verbose) {
        zsys_debug("%s: server sends: %s", self->name, command);
    }
}

static void
s_self_handle_client (zpnode_t *self, zsock_t* sock)
{
    zperf_msg_t* zpm = zperf_msg_new();
    zperf_msg_recv(zpm, sock);

    if (self->verbose) {
        zsys_debug("%s: client sends msg", self->name);
        zperf_msg_print(zpm);
    }

    zperf_msg_destroy(&zpm);
}

//  --------------------------------------------------------------------------
//  This is the actor which runs in its own thread.

void
zpnode_actor (zsock_t *pipe, void *args)
{
    zpnode_t * self = zpnode_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    while (!self->terminated) {
        zsock_t *which = (zsock_t *) zpoller_wait (self->poller, 0);
        if (!which) {
            continue;
        }
        if (which == self->pipe) {
            zpnode_recv_api (self);
            continue;
        }
        if (which == zyre_socket(self->zyre)) {
            zpnode_handle_zyre(self);
            continue;
        }
        if (self->server && which == zactor_sock(self->server)) {
            s_self_handle_server(self);
            continue;
        }

        s_self_handle_client(self, (zsock_t*)which);
        continue;
    }
    zpnode_destroy (&self);
}

//  --------------------------------------------------------------------------
//  Self test of this actor.

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
zpnode_test (bool verbose)
{
    zsys_init();
    zsys_debug("test zpnode: ");
    //  @selftest
    //  Simple create/destroy test

    // This test makes two nodes.  Both will provide a server and one
    // will provide connections to both servers.

    zactor_t *node1 = zactor_new (zpnode_actor, (void*)"zpnode1");
    assert (node1);
    zactor_t *node2 = zactor_new (zpnode_actor, (void*)"zpnode2");
    assert (node2);

    if (verbose) {
        zstr_send(node1, "VERBOSE");
        zstr_send(node2, "VERBOSE");
    }

    int rc = 0;
    zsock_send(node2, "ss", "SERVER", "zperf-server2");
    zsock_send(node2, "ss", "CONNECT", "zperf-server2"); // post connect
    zsock_send(node2, "ss", "CONNECT", "zperf-server1"); // pre connect
    zstr_send(node2, "START");

    zsock_send(node1, "ss", "SERVER", "zperf-server1");
    zstr_send(node1, "START");

    zclock_sleep(1000);

    zperf_msg_t* msg = zperf_msg_new();
    int nmsgs = 1000;
    size_t msgsize = 1024;

    zsock_send(node2, "sssi8", "LATENCY", "zperf-server1", "zperf-server2",
               nmsgs, msgsize);
    rc = zperf_msg_recv(msg, zactor_sock(node2));
    assert (rc == 0);
    zperf_msg_print(msg);

    rc = zperf_msg_recv(msg, zactor_sock(node2));
    assert (rc == 0);    
    zperf_msg_print(msg);

    // zsock_send(node2, "sssi8", "THROUGHPUT", "zperf-server1", "zperf-server2",
    //            nmsgs, msgsize);
    // rc = zperf_msg_recv(msg1, node2);
    // assert (rc == 0);
    // zperf_msg_print(msg);

    // rc = zperf_msg_recv(msg2, node2);
    // assert (rc == 0);    
    // zperf_msg_print(msg);

    zstr_send(node2, "STOP");
    zstr_send(node1, "STOP");

    zperf_msg_destroy (&msg);
    zactor_destroy (&node2);
    zactor_destroy (&node1);
    //  @end

    printf ("OK\n");
}
