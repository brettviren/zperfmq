/*  =========================================================================
    zperf_msg - The Zperf Protocol

    Codec class for zperf_msg.

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: zperf_msg.xml, or
     * The code generation script that built this file: zproto_codec_c
    ************************************************************************
    LGPL 3.0
    =========================================================================
*/

/*
@header
    zperf_msg - The Zperf Protocol
@discuss
@end
*/

#ifdef NDEBUG
#undef NDEBUG
#endif

#include "../include/zperf_msg.hpp"

//  Structure of our class

struct _zperf_msg_t {
    zframe_t *routing_id;               //  Routing_id from ROUTER, if any
    int id;                             //  zperf_msg message ID
    byte *needle;                       //  Read/write pointer for serialization
    byte *ceiling;                      //  Valid upper limit for read pointer
    char nickname [256];                //  Client nickname
    char mtype [256];                   //  Measurement type
    char stype [256];                   //  Socket type
    char action [256];                  //  Bind or Connect
    char endpoint [256];                //  Address
    uint32_t nmsgs;                     //  Number of messages
    uint64_t msgsize;                   //  Message size in bytes
    uint32_t timeout;                   //  Timeout in msec
    uint64_t time_us;                   //  Time elapsed in microseconds
    uint64_t cpu_us;                    //  CPU time used (user+system) in microseconds
    uint32_t noos;                      //  Number of out-of-order messages
    uint64_t nbytes;                    //  Number of bytes processed
    uint16_t status;                    //  3-digit status code
    char reason [256];                  //  Printable explanation
};

//  --------------------------------------------------------------------------
//  Network data encoding macros

//  Put a block of octets to the frame
#define PUT_OCTETS(host,size) { \
    memcpy (self->needle, (host), size); \
    self->needle += size; \
}

//  Get a block of octets from the frame
#define GET_OCTETS(host,size) { \
    if (self->needle + size > self->ceiling) { \
        zsys_warning ("zperf_msg: GET_OCTETS failed"); \
        goto malformed; \
    } \
    memcpy ((host), self->needle, size); \
    self->needle += size; \
}

//  Put a 1-byte number to the frame
#define PUT_NUMBER1(host) { \
    *(byte *) self->needle = (byte) (host); \
    self->needle++; \
}

//  Put a 2-byte number to the frame
#define PUT_NUMBER2(host) { \
    self->needle [0] = (byte) (((host) >> 8)  & 255); \
    self->needle [1] = (byte) (((host))       & 255); \
    self->needle += 2; \
}

//  Put a 4-byte number to the frame
#define PUT_NUMBER4(host) { \
    self->needle [0] = (byte) (((host) >> 24) & 255); \
    self->needle [1] = (byte) (((host) >> 16) & 255); \
    self->needle [2] = (byte) (((host) >> 8)  & 255); \
    self->needle [3] = (byte) (((host))       & 255); \
    self->needle += 4; \
}

//  Put a 8-byte number to the frame
#define PUT_NUMBER8(host) { \
    self->needle [0] = (byte) (((host) >> 56) & 255); \
    self->needle [1] = (byte) (((host) >> 48) & 255); \
    self->needle [2] = (byte) (((host) >> 40) & 255); \
    self->needle [3] = (byte) (((host) >> 32) & 255); \
    self->needle [4] = (byte) (((host) >> 24) & 255); \
    self->needle [5] = (byte) (((host) >> 16) & 255); \
    self->needle [6] = (byte) (((host) >> 8)  & 255); \
    self->needle [7] = (byte) (((host))       & 255); \
    self->needle += 8; \
}

//  Get a 1-byte number from the frame
#define GET_NUMBER1(host) { \
    if (self->needle + 1 > self->ceiling) { \
        zsys_warning ("zperf_msg: GET_NUMBER1 failed"); \
        goto malformed; \
    } \
    (host) = *(byte *) self->needle; \
    self->needle++; \
}

//  Get a 2-byte number from the frame
#define GET_NUMBER2(host) { \
    if (self->needle + 2 > self->ceiling) { \
        zsys_warning ("zperf_msg: GET_NUMBER2 failed"); \
        goto malformed; \
    } \
    (host) = ((uint16_t) (self->needle [0]) << 8) \
           +  (uint16_t) (self->needle [1]); \
    self->needle += 2; \
}

//  Get a 4-byte number from the frame
#define GET_NUMBER4(host) { \
    if (self->needle + 4 > self->ceiling) { \
        zsys_warning ("zperf_msg: GET_NUMBER4 failed"); \
        goto malformed; \
    } \
    (host) = ((uint32_t) (self->needle [0]) << 24) \
           + ((uint32_t) (self->needle [1]) << 16) \
           + ((uint32_t) (self->needle [2]) << 8) \
           +  (uint32_t) (self->needle [3]); \
    self->needle += 4; \
}

//  Get a 8-byte number from the frame
#define GET_NUMBER8(host) { \
    if (self->needle + 8 > self->ceiling) { \
        zsys_warning ("zperf_msg: GET_NUMBER8 failed"); \
        goto malformed; \
    } \
    (host) = ((uint64_t) (self->needle [0]) << 56) \
           + ((uint64_t) (self->needle [1]) << 48) \
           + ((uint64_t) (self->needle [2]) << 40) \
           + ((uint64_t) (self->needle [3]) << 32) \
           + ((uint64_t) (self->needle [4]) << 24) \
           + ((uint64_t) (self->needle [5]) << 16) \
           + ((uint64_t) (self->needle [6]) << 8) \
           +  (uint64_t) (self->needle [7]); \
    self->needle += 8; \
}

//  Put a string to the frame
#define PUT_STRING(host) { \
    size_t string_size = strlen (host); \
    PUT_NUMBER1 (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
}

//  Get a string from the frame
#define GET_STRING(host) { \
    size_t string_size; \
    GET_NUMBER1 (string_size); \
    if (self->needle + string_size > (self->ceiling)) { \
        zsys_warning ("zperf_msg: GET_STRING failed"); \
        goto malformed; \
    } \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
}

//  Put a long string to the frame
#define PUT_LONGSTR(host) { \
    size_t string_size = strlen (host); \
    PUT_NUMBER4 (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
}

//  Get a long string from the frame
#define GET_LONGSTR(host) { \
    size_t string_size; \
    GET_NUMBER4 (string_size); \
    if (self->needle + string_size > (self->ceiling)) { \
        zsys_warning ("zperf_msg: GET_LONGSTR failed"); \
        goto malformed; \
    } \
    free ((host)); \
    (host) = (char *) malloc (string_size + 1); \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
}

//  --------------------------------------------------------------------------
//  bytes cstring conversion macros

// create new array of unsigned char from properly encoded string
// len of resulting array is strlen (str) / 2
// caller is responsibe for freeing up the memory
#define BYTES_FROM_STR(dst, _str) { \
    char *str = (char*) (_str); \
    if (!str || (strlen (str) % 2) != 0) \
        return NULL; \
\
    size_t strlen_2 = strlen (str) / 2; \
    byte *mem = (byte*) zmalloc (strlen_2); \
    size_t i; \
\
    for (i = 0; i != strlen_2; i++) \
    { \
        char buff[3] = {0x0, 0x0, 0x0}; \
        strncpy (buff, str, 2); \
        unsigned int uint; \
        sscanf (buff, "%x", &uint); \
        assert (uint <= 0xff); \
        mem [i] = (byte) (0xff & uint); \
        str += 2; \
    } \
    dst = mem; \
}

// convert len bytes to hex string
// caller is responsibe for freeing up the memory
#define STR_FROM_BYTES(dst, _mem, _len) { \
    byte *mem = (byte*) (_mem); \
    size_t len = (size_t) (_len); \
    char* ret = (char*) zmalloc (2*len + 1); \
    char* aux = ret; \
    size_t i; \
    for (i = 0; i != len; i++) \
    { \
        sprintf (aux, "%02x", mem [i]); \
        aux+=2; \
    } \
    dst = ret; \
}

//  --------------------------------------------------------------------------
//  Create a new zperf_msg

zperf_msg_t *
zperf_msg_new (void)
{
    zperf_msg_t *self = (zperf_msg_t *) zmalloc (sizeof (zperf_msg_t));
    return self;
}

//  --------------------------------------------------------------------------
//  Create a new zperf_msg from zpl/zconfig_t *

zperf_msg_t *
    zperf_msg_new_zpl (zconfig_t *config)
{
    assert (config);
    zperf_msg_t *self = NULL;
    char *message = zconfig_get (config, "message", NULL);
    if (!message) {
        zsys_error ("Can't find 'message' section");
        return NULL;
    }

    if (streq ("ZPERF_MSG_HELLO", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_HELLO);
    }
    else
    if (streq ("ZPERF_MSG_HELLO_OK", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_HELLO_OK);
    }
    else
    if (streq ("ZPERF_MSG_SOCKET", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_SOCKET);
    }
    else
    if (streq ("ZPERF_MSG_SOCKET_OK", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_SOCKET_OK);
    }
    else
    if (streq ("ZPERF_MSG_MEASURE", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_MEASURE);
    }
    else
    if (streq ("ZPERF_MSG_RESULT", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_RESULT);
    }
    else
    if (streq ("ZPERF_MSG_PING", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_PING);
    }
    else
    if (streq ("ZPERF_MSG_PING_OK", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_PING_OK);
    }
    else
    if (streq ("ZPERF_MSG_GOODBYE", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_GOODBYE);
    }
    else
    if (streq ("ZPERF_MSG_GOODBYE_OK", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_GOODBYE_OK);
    }
    else
    if (streq ("ZPERF_MSG_ERROR", message)) {
        self = zperf_msg_new ();
        zperf_msg_set_id (self, ZPERF_MSG_ERROR);
    }
    else
       {
        zsys_error ("message=%s is not known", message);
        return NULL;
       }

    char *s = zconfig_get (config, "routing_id", NULL);
    if (s) {
        byte *bvalue;
        BYTES_FROM_STR (bvalue, s);
        if (!bvalue) {
            zperf_msg_destroy (&self);
            return NULL;
        }
        zframe_t *frame = zframe_new (bvalue, strlen (s) / 2);
        free (bvalue);
        self->routing_id = frame;
    }


    zconfig_t *content = NULL;
    switch (self->id) {
        case ZPERF_MSG_HELLO:
            content = zconfig_locate (config, "content");
            if (!content) {
                zsys_error ("Can't find 'content' section");
                zperf_msg_destroy (&self);
                return NULL;
            }
            {
            char *s = zconfig_get (content, "nickname", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->nickname, s, 255);
            }
            {
            char *s = zconfig_get (content, "mtype", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->mtype, s, 255);
            }
            {
            char *s = zconfig_get (content, "stype", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->stype, s, 255);
            }
            break;
        case ZPERF_MSG_HELLO_OK:
            content = zconfig_locate (config, "content");
            if (!content) {
                zsys_error ("Can't find 'content' section");
                zperf_msg_destroy (&self);
                return NULL;
            }
            {
            char *s = zconfig_get (content, "nickname", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->nickname, s, 255);
            }
            {
            char *s = zconfig_get (content, "mtype", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->mtype, s, 255);
            }
            {
            char *s = zconfig_get (content, "stype", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->stype, s, 255);
            }
            break;
        case ZPERF_MSG_SOCKET:
            content = zconfig_locate (config, "content");
            if (!content) {
                zsys_error ("Can't find 'content' section");
                zperf_msg_destroy (&self);
                return NULL;
            }
            {
            char *s = zconfig_get (content, "action", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->action, s, 255);
            }
            {
            char *s = zconfig_get (content, "endpoint", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->endpoint, s, 255);
            }
            break;
        case ZPERF_MSG_SOCKET_OK:
            content = zconfig_locate (config, "content");
            if (!content) {
                zsys_error ("Can't find 'content' section");
                zperf_msg_destroy (&self);
                return NULL;
            }
            {
            char *s = zconfig_get (content, "action", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->action, s, 255);
            }
            {
            char *s = zconfig_get (content, "endpoint", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->endpoint, s, 255);
            }
            break;
        case ZPERF_MSG_MEASURE:
            content = zconfig_locate (config, "content");
            if (!content) {
                zsys_error ("Can't find 'content' section");
                zperf_msg_destroy (&self);
                return NULL;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "nmsgs", NULL);
            if (!s) {
                zsys_error ("content/nmsgs not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/nmsgs: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->nmsgs = uvalue;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "msgsize", NULL);
            if (!s) {
                zsys_error ("content/msgsize not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/msgsize: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->msgsize = uvalue;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "timeout", NULL);
            if (!s) {
                zsys_error ("content/timeout not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/timeout: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->timeout = uvalue;
            }
            break;
        case ZPERF_MSG_RESULT:
            content = zconfig_locate (config, "content");
            if (!content) {
                zsys_error ("Can't find 'content' section");
                zperf_msg_destroy (&self);
                return NULL;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "nmsgs", NULL);
            if (!s) {
                zsys_error ("content/nmsgs not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/nmsgs: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->nmsgs = uvalue;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "msgsize", NULL);
            if (!s) {
                zsys_error ("content/msgsize not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/msgsize: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->msgsize = uvalue;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "timeout", NULL);
            if (!s) {
                zsys_error ("content/timeout not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/timeout: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->timeout = uvalue;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "time_us", NULL);
            if (!s) {
                zsys_error ("content/time_us not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/time_us: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->time_us = uvalue;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "cpu_us", NULL);
            if (!s) {
                zsys_error ("content/cpu_us not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/cpu_us: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->cpu_us = uvalue;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "noos", NULL);
            if (!s) {
                zsys_error ("content/noos not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/noos: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->noos = uvalue;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "nbytes", NULL);
            if (!s) {
                zsys_error ("content/nbytes not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/nbytes: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->nbytes = uvalue;
            }
            break;
        case ZPERF_MSG_PING:
            break;
        case ZPERF_MSG_PING_OK:
            break;
        case ZPERF_MSG_GOODBYE:
            break;
        case ZPERF_MSG_GOODBYE_OK:
            break;
        case ZPERF_MSG_ERROR:
            content = zconfig_locate (config, "content");
            if (!content) {
                zsys_error ("Can't find 'content' section");
                zperf_msg_destroy (&self);
                return NULL;
            }
            {
            char *es = NULL;
            char *s = zconfig_get (content, "status", NULL);
            if (!s) {
                zsys_error ("content/status not found");
                zperf_msg_destroy (&self);
                return NULL;
            }
            uint64_t uvalue = (uint64_t) strtoll (s, &es, 10);
            if (es != s+strlen (s)) {
                zsys_error ("content/status: %s is not a number", s);
                zperf_msg_destroy (&self);
                return NULL;
            }
            self->status = uvalue;
            }
            {
            char *s = zconfig_get (content, "reason", NULL);
            if (!s) {
                zperf_msg_destroy (&self);
                return NULL;
            }
            strncpy (self->reason, s, 255);
            }
            break;
    }
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zperf_msg

void
zperf_msg_destroy (zperf_msg_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zperf_msg_t *self = *self_p;

        //  Free class properties
        zframe_destroy (&self->routing_id);

        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Create a deep copy of a zperf_msg instance

zperf_msg_t *
zperf_msg_dup (zperf_msg_t *other)
{
    assert (other);
    zperf_msg_t *copy = zperf_msg_new ();

    // Copy the routing and message id
    zperf_msg_set_routing_id (copy, zperf_msg_routing_id (other));
    zperf_msg_set_id (copy, zperf_msg_id (other));


    // Copy the rest of the fields
    zperf_msg_set_nickname (copy, zperf_msg_nickname (other));
    zperf_msg_set_mtype (copy, zperf_msg_mtype (other));
    zperf_msg_set_stype (copy, zperf_msg_stype (other));
    zperf_msg_set_action (copy, zperf_msg_action (other));
    zperf_msg_set_endpoint (copy, zperf_msg_endpoint (other));
    zperf_msg_set_nmsgs (copy, zperf_msg_nmsgs (other));
    zperf_msg_set_msgsize (copy, zperf_msg_msgsize (other));
    zperf_msg_set_timeout (copy, zperf_msg_timeout (other));
    zperf_msg_set_time_us (copy, zperf_msg_time_us (other));
    zperf_msg_set_cpu_us (copy, zperf_msg_cpu_us (other));
    zperf_msg_set_noos (copy, zperf_msg_noos (other));
    zperf_msg_set_nbytes (copy, zperf_msg_nbytes (other));
    zperf_msg_set_status (copy, zperf_msg_status (other));
    zperf_msg_set_reason (copy, zperf_msg_reason (other));

    return copy;
}

//  --------------------------------------------------------------------------
//  Receive a zperf_msg from the socket. Returns 0 if OK, -1 if
//  the recv was interrupted, or -2 if the message is malformed.
//  Blocks if there is no message waiting.

int
zperf_msg_recv (zperf_msg_t *self, zsock_t *input)
{
    assert (input);
    int rc = 0;
    zmq_msg_t frame;
    zmq_msg_init (&frame);

    if (zsock_type (input) == ZMQ_ROUTER) {
        zframe_destroy (&self->routing_id);
        self->routing_id = zframe_recv (input);
        if (!self->routing_id || !zsock_rcvmore (input)) {
            zsys_warning ("zperf_msg: no routing ID");
            rc = -1;            //  Interrupted
            goto malformed;
        }
    }
    int size;
    size = zmq_msg_recv (&frame, zsock_resolve (input), 0);
    if (size == -1) {
        zsys_warning ("zperf_msg: interrupted");
        rc = -1;                //  Interrupted
        goto malformed;
    }
    self->needle = (byte *) zmq_msg_data (&frame);
    self->ceiling = self->needle + zmq_msg_size (&frame);


    //  Get and check protocol signature
    uint16_t signature;
    GET_NUMBER2 (signature);
    if (signature != (0xAAA0 | 0)) {
        zsys_warning ("zperf_msg: invalid signature");
        rc = -2;                //  Malformed
        goto malformed;
    }
    //  Get message id and parse per message type
    GET_NUMBER1 (self->id);

    switch (self->id) {
        case ZPERF_MSG_HELLO:
            GET_STRING (self->nickname);
            GET_STRING (self->mtype);
            GET_STRING (self->stype);
            break;

        case ZPERF_MSG_HELLO_OK:
            GET_STRING (self->nickname);
            GET_STRING (self->mtype);
            GET_STRING (self->stype);
            break;

        case ZPERF_MSG_SOCKET:
            GET_STRING (self->action);
            GET_STRING (self->endpoint);
            break;

        case ZPERF_MSG_SOCKET_OK:
            GET_STRING (self->action);
            GET_STRING (self->endpoint);
            break;

        case ZPERF_MSG_MEASURE:
            GET_NUMBER4 (self->nmsgs);
            GET_NUMBER8 (self->msgsize);
            GET_NUMBER4 (self->timeout);
            break;

        case ZPERF_MSG_RESULT:
            GET_NUMBER4 (self->nmsgs);
            GET_NUMBER8 (self->msgsize);
            GET_NUMBER8 (self->timeout);
            GET_NUMBER8 (self->time_us);
            GET_NUMBER8 (self->cpu_us);
            GET_NUMBER4 (self->noos);
            GET_NUMBER8 (self->nbytes);
            break;

        case ZPERF_MSG_PING:
            break;

        case ZPERF_MSG_PING_OK:
            break;

        case ZPERF_MSG_GOODBYE:
            break;

        case ZPERF_MSG_GOODBYE_OK:
            break;

        case ZPERF_MSG_ERROR:
            GET_NUMBER2 (self->status);
            GET_STRING (self->reason);
            break;

        default:
            zsys_warning ("zperf_msg: bad message ID");
            rc = -2;            //  Malformed
            goto malformed;
    }
    //  Successful return
    zmq_msg_close (&frame);
    return rc;

    //  Error returns
    malformed:
        zmq_msg_close (&frame);
        return rc;              //  Invalid message
}


//  --------------------------------------------------------------------------
//  Send the zperf_msg to the socket. Does not destroy it. Returns 0 if
//  OK, else -1.

int
zperf_msg_send (zperf_msg_t *self, zsock_t *output)
{
    assert (self);
    assert (output);

    if (zsock_type (output) == ZMQ_ROUTER)
        zframe_send (&self->routing_id, output, ZFRAME_MORE + ZFRAME_REUSE);

    size_t frame_size = 2 + 1;          //  Signature and message ID

    switch (self->id) {
        case ZPERF_MSG_HELLO:
            frame_size += 1 + strlen (self->nickname);
            frame_size += 1 + strlen (self->mtype);
            frame_size += 1 + strlen (self->stype);
            break;
        case ZPERF_MSG_HELLO_OK:
            frame_size += 1 + strlen (self->nickname);
            frame_size += 1 + strlen (self->mtype);
            frame_size += 1 + strlen (self->stype);
            break;
        case ZPERF_MSG_SOCKET:
            frame_size += 1 + strlen (self->action);
            frame_size += 1 + strlen (self->endpoint);
            break;
        case ZPERF_MSG_SOCKET_OK:
            frame_size += 1 + strlen (self->action);
            frame_size += 1 + strlen (self->endpoint);
            break;
        case ZPERF_MSG_MEASURE:
            frame_size += 4;            //  nmsgs
            frame_size += 8;            //  msgsize
            frame_size += 4;            //  timeout
            break;
        case ZPERF_MSG_RESULT:
            frame_size += 4;            //  nmsgs
            frame_size += 8;            //  msgsize
            frame_size += 8;            //  timeout
            frame_size += 8;            //  time_us
            frame_size += 8;            //  cpu_us
            frame_size += 4;            //  noos
            frame_size += 8;            //  nbytes
            break;
        case ZPERF_MSG_ERROR:
            frame_size += 2;            //  status
            frame_size += 1 + strlen (self->reason);
            break;
    }
    //  Now serialize message into the frame
    zmq_msg_t frame;
    zmq_msg_init_size (&frame, frame_size);
    self->needle = (byte *) zmq_msg_data (&frame);
    PUT_NUMBER2 (0xAAA0 | 0);
    PUT_NUMBER1 (self->id);
    size_t nbr_frames = 1;              //  Total number of frames to send

    switch (self->id) {
        case ZPERF_MSG_HELLO:
            PUT_STRING (self->nickname);
            PUT_STRING (self->mtype);
            PUT_STRING (self->stype);
            break;

        case ZPERF_MSG_HELLO_OK:
            PUT_STRING (self->nickname);
            PUT_STRING (self->mtype);
            PUT_STRING (self->stype);
            break;

        case ZPERF_MSG_SOCKET:
            PUT_STRING (self->action);
            PUT_STRING (self->endpoint);
            break;

        case ZPERF_MSG_SOCKET_OK:
            PUT_STRING (self->action);
            PUT_STRING (self->endpoint);
            break;

        case ZPERF_MSG_MEASURE:
            PUT_NUMBER4 (self->nmsgs);
            PUT_NUMBER8 (self->msgsize);
            PUT_NUMBER4 (self->timeout);
            break;

        case ZPERF_MSG_RESULT:
            PUT_NUMBER4 (self->nmsgs);
            PUT_NUMBER8 (self->msgsize);
            PUT_NUMBER8 (self->timeout);
            PUT_NUMBER8 (self->time_us);
            PUT_NUMBER8 (self->cpu_us);
            PUT_NUMBER4 (self->noos);
            PUT_NUMBER8 (self->nbytes);
            break;

        case ZPERF_MSG_ERROR:
            PUT_NUMBER2 (self->status);
            PUT_STRING (self->reason);
            break;

    }
    //  Now send the data frame
    zmq_msg_send (&frame, zsock_resolve (output), --nbr_frames? ZMQ_SNDMORE: 0);

    return 0;
}


//  --------------------------------------------------------------------------
//  Print contents of message to stdout

void
zperf_msg_print (zperf_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case ZPERF_MSG_HELLO:
            zsys_debug ("ZPERF_MSG_HELLO:");
            zsys_debug ("    nickname='%s'", self->nickname);
            zsys_debug ("    mtype='%s'", self->mtype);
            zsys_debug ("    stype='%s'", self->stype);
            break;

        case ZPERF_MSG_HELLO_OK:
            zsys_debug ("ZPERF_MSG_HELLO_OK:");
            zsys_debug ("    nickname='%s'", self->nickname);
            zsys_debug ("    mtype='%s'", self->mtype);
            zsys_debug ("    stype='%s'", self->stype);
            break;

        case ZPERF_MSG_SOCKET:
            zsys_debug ("ZPERF_MSG_SOCKET:");
            zsys_debug ("    action='%s'", self->action);
            zsys_debug ("    endpoint='%s'", self->endpoint);
            break;

        case ZPERF_MSG_SOCKET_OK:
            zsys_debug ("ZPERF_MSG_SOCKET_OK:");
            zsys_debug ("    action='%s'", self->action);
            zsys_debug ("    endpoint='%s'", self->endpoint);
            break;

        case ZPERF_MSG_MEASURE:
            zsys_debug ("ZPERF_MSG_MEASURE:");
            zsys_debug ("    nmsgs=%ld", (long) self->nmsgs);
            zsys_debug ("    msgsize=%ld", (long) self->msgsize);
            zsys_debug ("    timeout=%ld", (long) self->timeout);
            break;

        case ZPERF_MSG_RESULT:
            zsys_debug ("ZPERF_MSG_RESULT:");
            zsys_debug ("    nmsgs=%ld", (long) self->nmsgs);
            zsys_debug ("    msgsize=%ld", (long) self->msgsize);
            zsys_debug ("    timeout=%ld", (long) self->timeout);
            zsys_debug ("    time_us=%ld", (long) self->time_us);
            zsys_debug ("    cpu_us=%ld", (long) self->cpu_us);
            zsys_debug ("    noos=%ld", (long) self->noos);
            zsys_debug ("    nbytes=%ld", (long) self->nbytes);
            break;

        case ZPERF_MSG_PING:
            zsys_debug ("ZPERF_MSG_PING:");
            break;

        case ZPERF_MSG_PING_OK:
            zsys_debug ("ZPERF_MSG_PING_OK:");
            break;

        case ZPERF_MSG_GOODBYE:
            zsys_debug ("ZPERF_MSG_GOODBYE:");
            break;

        case ZPERF_MSG_GOODBYE_OK:
            zsys_debug ("ZPERF_MSG_GOODBYE_OK:");
            break;

        case ZPERF_MSG_ERROR:
            zsys_debug ("ZPERF_MSG_ERROR:");
            zsys_debug ("    status=%ld", (long) self->status);
            zsys_debug ("    reason='%s'", self->reason);
            break;

    }
}

//  --------------------------------------------------------------------------
//  Export class as zconfig_t*. Caller is responsibe for destroying the instance

zconfig_t *
zperf_msg_zpl (zperf_msg_t *self, zconfig_t *parent)
{
    assert (self);

    zconfig_t *root = zconfig_new ("zperf_msg", parent);

    switch (self->id) {
        case ZPERF_MSG_HELLO:
        {
            zconfig_put (root, "message", "ZPERF_MSG_HELLO");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            zconfig_t *config = zconfig_new ("content", root);
            zconfig_putf (config, "nickname", "%s", self->nickname);
            zconfig_putf (config, "mtype", "%s", self->mtype);
            zconfig_putf (config, "stype", "%s", self->stype);
            break;
            }
        case ZPERF_MSG_HELLO_OK:
        {
            zconfig_put (root, "message", "ZPERF_MSG_HELLO_OK");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            zconfig_t *config = zconfig_new ("content", root);
            zconfig_putf (config, "nickname", "%s", self->nickname);
            zconfig_putf (config, "mtype", "%s", self->mtype);
            zconfig_putf (config, "stype", "%s", self->stype);
            break;
            }
        case ZPERF_MSG_SOCKET:
        {
            zconfig_put (root, "message", "ZPERF_MSG_SOCKET");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            zconfig_t *config = zconfig_new ("content", root);
            zconfig_putf (config, "action", "%s", self->action);
            zconfig_putf (config, "endpoint", "%s", self->endpoint);
            break;
            }
        case ZPERF_MSG_SOCKET_OK:
        {
            zconfig_put (root, "message", "ZPERF_MSG_SOCKET_OK");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            zconfig_t *config = zconfig_new ("content", root);
            zconfig_putf (config, "action", "%s", self->action);
            zconfig_putf (config, "endpoint", "%s", self->endpoint);
            break;
            }
        case ZPERF_MSG_MEASURE:
        {
            zconfig_put (root, "message", "ZPERF_MSG_MEASURE");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            zconfig_t *config = zconfig_new ("content", root);
            zconfig_putf (config, "nmsgs", "%ld", (long) self->nmsgs);
            zconfig_putf (config, "msgsize", "%ld", (long) self->msgsize);
            zconfig_putf (config, "timeout", "%ld", (long) self->timeout);
            break;
            }
        case ZPERF_MSG_RESULT:
        {
            zconfig_put (root, "message", "ZPERF_MSG_RESULT");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            zconfig_t *config = zconfig_new ("content", root);
            zconfig_putf (config, "nmsgs", "%ld", (long) self->nmsgs);
            zconfig_putf (config, "msgsize", "%ld", (long) self->msgsize);
            zconfig_putf (config, "timeout", "%ld", (long) self->timeout);
            zconfig_putf (config, "time_us", "%ld", (long) self->time_us);
            zconfig_putf (config, "cpu_us", "%ld", (long) self->cpu_us);
            zconfig_putf (config, "noos", "%ld", (long) self->noos);
            zconfig_putf (config, "nbytes", "%ld", (long) self->nbytes);
            break;
            }
        case ZPERF_MSG_PING:
        {
            zconfig_put (root, "message", "ZPERF_MSG_PING");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            break;
            }
        case ZPERF_MSG_PING_OK:
        {
            zconfig_put (root, "message", "ZPERF_MSG_PING_OK");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            break;
            }
        case ZPERF_MSG_GOODBYE:
        {
            zconfig_put (root, "message", "ZPERF_MSG_GOODBYE");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            break;
            }
        case ZPERF_MSG_GOODBYE_OK:
        {
            zconfig_put (root, "message", "ZPERF_MSG_GOODBYE_OK");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            break;
            }
        case ZPERF_MSG_ERROR:
        {
            zconfig_put (root, "message", "ZPERF_MSG_ERROR");

            if (self->routing_id) {
                char *hex = NULL;
                STR_FROM_BYTES (hex, zframe_data (self->routing_id), zframe_size (self->routing_id));
                zconfig_putf (root, "routing_id", "%s", hex);
                zstr_free (&hex);
            }


            zconfig_t *config = zconfig_new ("content", root);
            zconfig_putf (config, "status", "%ld", (long) self->status);
            zconfig_putf (config, "reason", "%s", self->reason);
            break;
            }
    }
    return root;
}

//  --------------------------------------------------------------------------
//  Get/set the message routing_id

zframe_t *
zperf_msg_routing_id (zperf_msg_t *self)
{
    assert (self);
    return self->routing_id;
}

void
zperf_msg_set_routing_id (zperf_msg_t *self, zframe_t *routing_id)
{
    if (self->routing_id)
        zframe_destroy (&self->routing_id);
    self->routing_id = zframe_dup (routing_id);
}


//  --------------------------------------------------------------------------
//  Get/set the zperf_msg id

int
zperf_msg_id (zperf_msg_t *self)
{
    assert (self);
    return self->id;
}

void
zperf_msg_set_id (zperf_msg_t *self, int id)
{
    self->id = id;
}

//  --------------------------------------------------------------------------
//  Return a printable command string

const char *
zperf_msg_command (zperf_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case ZPERF_MSG_HELLO:
            return ("HELLO");
            break;
        case ZPERF_MSG_HELLO_OK:
            return ("HELLO_OK");
            break;
        case ZPERF_MSG_SOCKET:
            return ("SOCKET");
            break;
        case ZPERF_MSG_SOCKET_OK:
            return ("SOCKET_OK");
            break;
        case ZPERF_MSG_MEASURE:
            return ("MEASURE");
            break;
        case ZPERF_MSG_RESULT:
            return ("RESULT");
            break;
        case ZPERF_MSG_PING:
            return ("PING");
            break;
        case ZPERF_MSG_PING_OK:
            return ("PING_OK");
            break;
        case ZPERF_MSG_GOODBYE:
            return ("GOODBYE");
            break;
        case ZPERF_MSG_GOODBYE_OK:
            return ("GOODBYE_OK");
            break;
        case ZPERF_MSG_ERROR:
            return ("ERROR");
            break;
    }
    return "?";
}


//  --------------------------------------------------------------------------
//  Get/set the nickname field

const char *
zperf_msg_nickname (zperf_msg_t *self)
{
    assert (self);
    return self->nickname;
}

void
zperf_msg_set_nickname (zperf_msg_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->nickname)
        return;
    strncpy (self->nickname, value, 255);
    self->nickname [255] = 0;
}


//  --------------------------------------------------------------------------
//  Get/set the mtype field

const char *
zperf_msg_mtype (zperf_msg_t *self)
{
    assert (self);
    return self->mtype;
}

void
zperf_msg_set_mtype (zperf_msg_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->mtype)
        return;
    strncpy (self->mtype, value, 255);
    self->mtype [255] = 0;
}


//  --------------------------------------------------------------------------
//  Get/set the stype field

const char *
zperf_msg_stype (zperf_msg_t *self)
{
    assert (self);
    return self->stype;
}

void
zperf_msg_set_stype (zperf_msg_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->stype)
        return;
    strncpy (self->stype, value, 255);
    self->stype [255] = 0;
}


//  --------------------------------------------------------------------------
//  Get/set the action field

const char *
zperf_msg_action (zperf_msg_t *self)
{
    assert (self);
    return self->action;
}

void
zperf_msg_set_action (zperf_msg_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->action)
        return;
    strncpy (self->action, value, 255);
    self->action [255] = 0;
}


//  --------------------------------------------------------------------------
//  Get/set the endpoint field

const char *
zperf_msg_endpoint (zperf_msg_t *self)
{
    assert (self);
    return self->endpoint;
}

void
zperf_msg_set_endpoint (zperf_msg_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->endpoint)
        return;
    strncpy (self->endpoint, value, 255);
    self->endpoint [255] = 0;
}


//  --------------------------------------------------------------------------
//  Get/set the nmsgs field

uint32_t
zperf_msg_nmsgs (zperf_msg_t *self)
{
    assert (self);
    return self->nmsgs;
}

void
zperf_msg_set_nmsgs (zperf_msg_t *self, uint32_t nmsgs)
{
    assert (self);
    self->nmsgs = nmsgs;
}


//  --------------------------------------------------------------------------
//  Get/set the msgsize field

uint64_t
zperf_msg_msgsize (zperf_msg_t *self)
{
    assert (self);
    return self->msgsize;
}

void
zperf_msg_set_msgsize (zperf_msg_t *self, uint64_t msgsize)
{
    assert (self);
    self->msgsize = msgsize;
}


//  --------------------------------------------------------------------------
//  Get/set the timeout field

uint32_t
zperf_msg_timeout (zperf_msg_t *self)
{
    assert (self);
    return self->timeout;
}

void
zperf_msg_set_timeout (zperf_msg_t *self, uint32_t timeout)
{
    assert (self);
    self->timeout = timeout;
}


//  --------------------------------------------------------------------------
//  Get/set the time_us field

uint64_t
zperf_msg_time_us (zperf_msg_t *self)
{
    assert (self);
    return self->time_us;
}

void
zperf_msg_set_time_us (zperf_msg_t *self, uint64_t time_us)
{
    assert (self);
    self->time_us = time_us;
}


//  --------------------------------------------------------------------------
//  Get/set the cpu_us field

uint64_t
zperf_msg_cpu_us (zperf_msg_t *self)
{
    assert (self);
    return self->cpu_us;
}

void
zperf_msg_set_cpu_us (zperf_msg_t *self, uint64_t cpu_us)
{
    assert (self);
    self->cpu_us = cpu_us;
}


//  --------------------------------------------------------------------------
//  Get/set the noos field

uint32_t
zperf_msg_noos (zperf_msg_t *self)
{
    assert (self);
    return self->noos;
}

void
zperf_msg_set_noos (zperf_msg_t *self, uint32_t noos)
{
    assert (self);
    self->noos = noos;
}


//  --------------------------------------------------------------------------
//  Get/set the nbytes field

uint64_t
zperf_msg_nbytes (zperf_msg_t *self)
{
    assert (self);
    return self->nbytes;
}

void
zperf_msg_set_nbytes (zperf_msg_t *self, uint64_t nbytes)
{
    assert (self);
    self->nbytes = nbytes;
}


//  --------------------------------------------------------------------------
//  Get/set the status field

uint16_t
zperf_msg_status (zperf_msg_t *self)
{
    assert (self);
    return self->status;
}

void
zperf_msg_set_status (zperf_msg_t *self, uint16_t status)
{
    assert (self);
    self->status = status;
}


//  --------------------------------------------------------------------------
//  Get/set the reason field

const char *
zperf_msg_reason (zperf_msg_t *self)
{
    assert (self);
    return self->reason;
}

void
zperf_msg_set_reason (zperf_msg_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->reason)
        return;
    strncpy (self->reason, value, 255);
    self->reason [255] = 0;
}



//  --------------------------------------------------------------------------
//  Selftest

void
zperf_msg_test (bool verbose)
{
    printf (" * zperf_msg: ");

    if (verbose)
        printf ("\n");

    //  @selftest
    //  Simple create/destroy test
    zconfig_t *config;
    zperf_msg_t *self = zperf_msg_new ();
    assert (self);
    zperf_msg_destroy (&self);
    //  Create pair of sockets we can send through
    //  We must bind before connect if we wish to remain compatible with ZeroMQ < v4
    zsock_t *output = zsock_new (ZMQ_DEALER);
    assert (output);
    int rc = zsock_bind (output, "inproc://selftest-zperf_msg");
    assert (rc == 0);

    zsock_t *input = zsock_new (ZMQ_ROUTER);
    assert (input);
    rc = zsock_connect (input, "inproc://selftest-zperf_msg");
    assert (rc == 0);


    //  Encode/send/decode and verify each message type
    int instance;
    self = zperf_msg_new ();
    zperf_msg_set_id (self, ZPERF_MSG_HELLO);
    zperf_msg_set_nickname (self, "Life is short but Now lasts for ever");
    zperf_msg_set_mtype (self, "Life is short but Now lasts for ever");
    zperf_msg_set_stype (self, "Life is short but Now lasts for ever");
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        assert (streq (zperf_msg_nickname (self), "Life is short but Now lasts for ever"));
        assert (streq (zperf_msg_mtype (self), "Life is short but Now lasts for ever"));
        assert (streq (zperf_msg_stype (self), "Life is short but Now lasts for ever"));
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }
    zperf_msg_set_id (self, ZPERF_MSG_HELLO_OK);
    zperf_msg_set_nickname (self, "Life is short but Now lasts for ever");
    zperf_msg_set_mtype (self, "Life is short but Now lasts for ever");
    zperf_msg_set_stype (self, "Life is short but Now lasts for ever");
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        assert (streq (zperf_msg_nickname (self), "Life is short but Now lasts for ever"));
        assert (streq (zperf_msg_mtype (self), "Life is short but Now lasts for ever"));
        assert (streq (zperf_msg_stype (self), "Life is short but Now lasts for ever"));
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }
    zperf_msg_set_id (self, ZPERF_MSG_SOCKET);
    zperf_msg_set_action (self, "Life is short but Now lasts for ever");
    zperf_msg_set_endpoint (self, "Life is short but Now lasts for ever");
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        assert (streq (zperf_msg_action (self), "Life is short but Now lasts for ever"));
        assert (streq (zperf_msg_endpoint (self), "Life is short but Now lasts for ever"));
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }
    zperf_msg_set_id (self, ZPERF_MSG_SOCKET_OK);
    zperf_msg_set_action (self, "Life is short but Now lasts for ever");
    zperf_msg_set_endpoint (self, "Life is short but Now lasts for ever");
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        assert (streq (zperf_msg_action (self), "Life is short but Now lasts for ever"));
        assert (streq (zperf_msg_endpoint (self), "Life is short but Now lasts for ever"));
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }
    zperf_msg_set_id (self, ZPERF_MSG_MEASURE);
    zperf_msg_set_nmsgs (self, 123);
    zperf_msg_set_msgsize (self, 123);
    zperf_msg_set_timeout (self, 123);
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        assert (zperf_msg_nmsgs (self) == 123);
        assert (zperf_msg_msgsize (self) == 123);
        assert (zperf_msg_timeout (self) == 123);
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }
    zperf_msg_set_id (self, ZPERF_MSG_RESULT);
    zperf_msg_set_nmsgs (self, 123);
    zperf_msg_set_msgsize (self, 123);
    zperf_msg_set_timeout (self, 123);
    zperf_msg_set_time_us (self, 123);
    zperf_msg_set_cpu_us (self, 123);
    zperf_msg_set_noos (self, 123);
    zperf_msg_set_nbytes (self, 123);
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        assert (zperf_msg_nmsgs (self) == 123);
        assert (zperf_msg_msgsize (self) == 123);
        assert (zperf_msg_timeout (self) == 123);
        assert (zperf_msg_time_us (self) == 123);
        assert (zperf_msg_cpu_us (self) == 123);
        assert (zperf_msg_noos (self) == 123);
        assert (zperf_msg_nbytes (self) == 123);
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }
    zperf_msg_set_id (self, ZPERF_MSG_PING);
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }
    zperf_msg_set_id (self, ZPERF_MSG_PING_OK);
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }
    zperf_msg_set_id (self, ZPERF_MSG_GOODBYE);
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }
    zperf_msg_set_id (self, ZPERF_MSG_GOODBYE_OK);
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }
    zperf_msg_set_id (self, ZPERF_MSG_ERROR);
    zperf_msg_set_status (self, 123);
    zperf_msg_set_reason (self, "Life is short but Now lasts for ever");
    // convert to zpl
    config = zperf_msg_zpl (self, NULL);
    if (verbose)
        zconfig_print (config);
    //  Send twice
    zperf_msg_send (self, output);
    zperf_msg_send (self, output);

    for (instance = 0; instance < 3; instance++) {
        zperf_msg_t *self_temp = self;
        if (instance < 2)
            zperf_msg_recv (self, input);
        else {
            self = zperf_msg_new_zpl (config);
            assert (self);
            zconfig_destroy (&config);
        }
        if (instance < 2)
            assert (zperf_msg_routing_id (self));
        assert (zperf_msg_status (self) == 123);
        assert (streq (zperf_msg_reason (self), "Life is short but Now lasts for ever"));
        if (instance == 2) {
            zperf_msg_destroy (&self);
            self = self_temp;
        }
    }

    zperf_msg_destroy (&self);
    zsock_destroy (&input);
    zsock_destroy (&output);
#if defined (__WINDOWS__)
    zsys_shutdown();
#endif
    //  @end

    printf ("OK\n");
}
