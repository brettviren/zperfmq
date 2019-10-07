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
    if (meas == "echo") {
        return zperf_echo(zperf, nmsgs);
    }
    if (meas == "yodel") {
        return zperf_yodel(zperf, nmsgs, msgsize);
    }
    if (meas == "send") {
        return zperf_send(zperf, nmsgs, msgsize);
    }
    if (meas == "recv") {
        return zperf_recv(zperf, nmsgs);
    }
    throw std::runtime_error("unkonwn measurement");
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

    std::string meas="echo";
    app.add_option("-m,--measurement", meas,
                   "Measurement (def=echo)");

    std::string stype="REP";
    app.add_option("-S,--socket-type", stype,
                   "Socket type (def=REP)");

    std::string outfile="zperf.json";
    app.add_option("-o,--output", outfile,
                   "Output file name (def=zperf.json)");

    CLI11_PARSE(app, argc, argv);

    json res = {
        {"socket_type", stype},
        {"measurement", meas},
    };

    const int snum = socket_type(stype);
    res["beg_us"] = now();
    zperf_t* zperf = zperf_new(snum);

    if (connect == "") {
        res["attachment"] = "bind";
        std::string ep = zperf_bind(zperf, bind.c_str());
        res["endpoint"] = ep;
        // spit out in case user needs the qualified address to connect other instance
        std::cerr << "bind: " << ep << std::endl;
    }
    else {
        res["attachment"] = "connect";
        res["endpoint"] = connect;
        zperf_connect(zperf, connect.c_str());
    }

    res["time_us"]  = run_it(zperf, meas, nmsgs, msgsize);
    res["noos"] = zperf_noos(zperf);
    res["nbytes"] = zperf_bytes(zperf);
    res["cpu_us"] = zperf_cpu(zperf);

    zperf_destroy(&zperf);
    res["end_us"] = now();

    if (outfile == "-" ) {
        std::cout << res.dump(4) << std::endl;
    }
    else {
        std::ofstream fstr(outfile);
        fstr << res.dump(4) << std::endl;
    }

    return 0;
}
