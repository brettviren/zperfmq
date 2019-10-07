/*  =========================================================================
    perf - A swish army knife of perf services.

It can provide both ends of a fast echo for latency measurements
and a sender or receiver of a fast flow for throughput measurements.

Measurements are initiated via command messages with results
returned.  While a measurement is ongoing the actor pipe is not
serviced.

    LGPL 3.0
    =========================================================================
*/

#ifndef PERF_H_INCLUDED
#define PERF_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


//  @interface
//  Create new perf actor instance.
//  @TODO: Describe the purpose of this actor!
//
//      zactor_t *perf = zactor_new (perf, NULL);
//
//  Destroy perf instance.
//
//      zactor_destroy (&perf);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (perf, "VERBOSE");
//
//  Start perf actor.
//
//      zstr_sendx (perf, "START", NULL);
//
//  Stop perf actor.
//
//      zstr_sendx (perf, "STOP", NULL);
//
//  This is the perf constructor as a zactor_fn;
ZPERFMQ_EXPORT void
    perf_actor (zsock_t *pipe, void *args);

//  Self test of this actor
ZPERFMQ_EXPORT void
    perf_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
