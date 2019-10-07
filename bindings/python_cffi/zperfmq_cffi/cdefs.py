################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
import re
zperfmq_cdefs = list ()
# Custom setup for zperfmq


#Import definitions from dependent projects

zperfmq_cdefs.append ('''
typedef struct _zperf_t zperf_t;
// CLASS: zperf
// Create a new zperf using the given socket type.
zperf_t *
    zperf_new (int socket_type);

// Destroy the zperf.
void
    zperf_destroy (zperf_t **self_p);

// Bind the zperf measurement socket to an address.
// Return the qualified address or NULL on error.
const char *
    zperf_bind (zperf_t *self, const char *address);

// Connect the zperf measurement socket to a fully qualified address.
// Return code is zero on success.
int
    zperf_connect (zperf_t *self, const char *address);

// Perform an echo measurement, expecting a given number of
// messages. Return elapsed operation time in microseconds.
uint64_t
    zperf_echo (zperf_t *self, int nmsgs);

// Initiate an echo measurement, expecting a given number of
// messages.
void
    zperf_echo_ini (zperf_t *self, int nmsgs);

// Finalize a previously initiated echo measurement, return elapsed
// operation time in microseconds.
uint64_t
    zperf_echo_fin (zperf_t *self);

// Perform a yodel measurement which sends the given number of
// messages of given size to an echo service.  Return elapsed
// operation time in microseconds.
uint64_t
    zperf_yodel (zperf_t *self, int nmsgs, uint64_t msgsize);

// Initiate a yodel measurement which sends the given number of
// messages of given size to an echo service.
void
    zperf_yodel_ini (zperf_t *self, int nmsgs, uint64_t msgsize);

// Finalize a previously initialized yodel measurement, return
// elapsed operation time in microseconds.
uint64_t
    zperf_yodel_fin (zperf_t *self);

// Perform a send measurement sending the given number of message of
// given size to a receiver.  Return elapsed operation time in
// microseconds.
uint64_t
    zperf_send (zperf_t *self, int nmsgs, uint64_t msgsize);

// Initialize a send measurement sending the given number of message
// of given size to a receiver.
void
    zperf_send_ini (zperf_t *self, int nmsgs, uint64_t msgsize);

// Finalize a previously initialized send measurement.  Return
// elapsed operation time in microseconds.
uint64_t
    zperf_send_fin (zperf_t *self);

// Perform a recv measurement recving the given number of messages.
// Return elapsed operation time in microseconds.
uint64_t
    zperf_recv (zperf_t *self, int nmsgs);

// Initialize a recv measurement.
void
    zperf_recv_ini (zperf_t *self, int nmsgs);

// Finalize a previously initialized recv measurement.  Return
// elapsed operation time in microseconds.
uint64_t
    zperf_recv_fin (zperf_t *self);

// Return the number of messages that were received out of sync
// during the previous yodel or recv measurements.  The measurement
// must be finalized.
int
    zperf_noos (zperf_t *self);

// Return the number of bytes transferred by the previous
// measurement.  The measurement must be finalized.
uint64_t
    zperf_bytes (zperf_t *self);

// Return the CPU time (user+system) in microseconds used by the last
// measurement.  The measurement must be finalized.
uint64_t
    zperf_cpu (zperf_t *self);

// Self test of this class.
void
    zperf_test (bool verbose);

''')
for i, item in enumerate (zperfmq_cdefs):
    zperfmq_cdefs [i] = re.sub(r';[^;]*\bva_list\b[^;]*;', ';', item, flags=re.S) # we don't support anything with a va_list arg

