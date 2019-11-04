/*  =========================================================================
    zperfcli - description

    LGPL 3.0
    =========================================================================
*/

/*
@header
    zperfcli -
@discuss
@end
*/

#include "zperfmq_classes.hpp"
#include <zyre.h>

// https://github.com/CLIUtils/CLI11
#include "CLI11.hpp"

// https://github.com/nlohmann/json
#include "json.hpp"

#include <chrono>
#include <iostream>
#include <algorithm>
#include <unordered_map>

using json = nlohmann::json;

static
uint64_t now()
{
    // quite the mouthful
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


static
int socket_type(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    std::unordered_map<std::string, int> zmqtypes {
        {"pair", 0},
        {"pub", 1},
        {"sub", 2},
        {"req", 3},
        {"rep", 4},
        {"dealer", 5},
        {"router", 6},
        {"pull", 7},
        {"push", 8},
        {"xpub", 9},
        {"xsub", 10},
        {"stream", 11}
    };
    int n = zmqtypes[name];
    return n;
}

static
uint64_t run_it(zperf_t* zperf, const std::string& meas, int nmsgs, size_t msgsize) {
    return zperf_measure(zperf, meas.c_str(), nmsgs, msgsize);
}

static
void wait_for_peer_exit(zyre_t* zyre, std::string want_name)
{
    std::string want_uuid = "";

    zpoller_t* poller = zpoller_new(zyre_socket(zyre), NULL);
    while (true) {
        void* which = zpoller_wait(poller, -1);
        if (!which) {
            break;
        }
        assert (which == zyre_socket(zyre));

        bool found_it = false;
        zyre_event_t *event = zyre_event_new (zyre);
        // zyre_event_print(event);
        const char* event_type = zyre_event_type(event);

        if (streq(event_type, "ENTER")) {
            std::string got_name = zyre_event_peer_name(event);
            if (got_name == want_name) {
                want_uuid = zyre_event_peer_uuid(event);
            }
        }
        else if (streq(event_type, "EXIT")) {
            std::string got_uuid = zyre_event_peer_uuid(event);
            // std::cerr << "got:" << got_uuid << " want:" << want_uuid << std::endl;
            if (got_uuid == want_uuid) {
                found_it = true;
            }
        }
        zyre_event_destroy (&event);                
        if (found_it) {
            break;
        }
    }

    zpoller_destroy(&poller);
}


int main (int argc, char *argv [])
{
    CLI::App app{"ZeroMQ performance measurement"};

    int niothreads = 1;
    app.add_option("-t,--niothreads", niothreads,
                   "Number of ZeroMQ I/O threads to use (def=1)");
    int nconnects = 1;
    app.add_option("-j,--nconnects", nconnects,
                   "Number of simultaneous socket connections (def=1)");
    int nmsgs = 1000;
    app.add_option("-n,--nmsgs", nmsgs,
                   "Number of messages (def=1000)");
    size_t msgsize = 1024;
    app.add_option("-s,--msgsize", msgsize,
                   "Message size in bytes (def=1024)");

    std::string bind="tcp://127.0.0.1:*";
    app.add_option("-b,--bind", bind,
                   "Address to bind");
    std::string connect="";
    app.add_option("-c,--connect", connect,
                   "Address to connect");

    std::string meas="ECHO";
    app.add_option("-m,--measurement", meas,
                   "Measurement (def=ECHO)");

    std::string stype="REP";
    app.add_option("-S,--socket-type", stype,
                   "Socket type (def=REP)");

    std::string outfile="zperf.json";
    app.add_option("-o,--output", outfile,
                   "Output file name (def=zperf.json)");

    int sleeps = 0;
    app.add_option("--sleeps", sleeps,
                   "Sleep for this many seconds after measurement complete, def=0");

    std::string nic="*";
    app.add_option("--nic", nic,
                   "Set the NIC on which Zyre shall chat");

    std::string zyre_name="zperf";
    app.add_option("--name", zyre_name,
                   "My zyre node name");

    std::string wait_for="";
    app.add_option("--wait", wait_for,
                   "If given, wait for this zyre node to exit before exiting");

    int verbose = 0;
    app.add_option("--verbose", verbose,
                   "Set verbosity level, def=0");

    int batch = 0;
    app.add_option("--batch", batch,
                   "Set batch buffer (experimental only)");

    CLI11_PARSE(app, argc, argv);

    zsys_init();
    if (niothreads > 1) {
        zsys_set_io_threads (niothreads);
    }

    zyre_t* zyre = zyre_new(zyre_name.c_str());
    if (verbose > 0) {
        zyre_set_verbose(zyre);
    }
    zyre_set_interface(zyre, nic.c_str());
    zyre_set_port(zyre, 5670);
    zyre_set_name(zyre, zyre_name.c_str());
    zyre_set_header(zyre, "ZPERF-MEASURE", meas.c_str());
    zyre_set_header(zyre, "ZPERF-SOCKET", stype.c_str());
    if (connect == "") {
        zyre_set_header(zyre, "ZPERF-BORC", "BIND");
        zyre_set_header(zyre, "ZPERF-ADDRESS", bind.c_str());    
    }
    else {
        zyre_set_header(zyre, "ZPERF-BORC", "CONNECT");
        zyre_set_header(zyre, "ZPERF-ADDRESS", connect.c_str());
    }
    zyre_start(zyre);

    json res = {
        {"socket_type", stype},
        {"measurement", meas},
        {"niothreads", niothreads},
        {"nconnects", nconnects}
    };

    const int snum = socket_type(stype);
    res["beg_us"] = now();
    zperf_t* zperf = zperf_new(snum);

    if (batch) {
        if (verbose) {
            zsys_warning("Setting BATCH buffer sizes to %d" , batch);
        }
        int rc = zperf_set_batch(zperf, batch);
        assert (rc == 0);
    }


    if (connect == "") {
        res["attachment"] = "bind";
        const char* epp = zperf_bind(zperf, bind.c_str());
        if (!epp) {
            std::cerr << "failed to bind: " << bind << std::endl;            
            throw std::runtime_error("failed to bind");
        }
        std::string ep = epp;
        res["endpoint"] = ep;
        // spit out in case user needs the qualified address to connect other instance
        std::cerr << "bind: " << ep << std::endl;
    }
    else {
        res["attachment"] = "connect";
        res["endpoint"] = connect;
        for (int count=0; count<nconnects; ++count) {
            zperf_connect(zperf, connect.c_str());
        }
    }


    res["time_us"]  = run_it(zperf, meas, nmsgs, msgsize);
    res["noos"] = zperf_noos(zperf);
    res["nbytes"] = zperf_bytes(zperf);
    res["cpu_us"] = zperf_cpu(zperf);

    while (sleeps) {
        if (verbose) {
            zsys_debug("sleeping %s", sleeps);
        }
        zclock_sleep(1000);
        --sleeps;
    }

    if (wait_for.size() > 0) {
        wait_for_peer_exit(zyre, wait_for);
    }
    

    zperf_destroy(&zperf);
    res["end_us"] = now();

    zyre_stop(zyre);
    zclock_sleep(1000);

    zyre_destroy(&zyre);

    if (outfile == "-" ) {
        std::cout << res.dump(4) << std::endl;
    }
    else {
        std::ofstream fstr(outfile);
        fstr << res.dump(4) << std::endl;
    }

    return 0;
}
