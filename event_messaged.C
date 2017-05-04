#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include "event_messaged_sdbus.h"

using namespace std;

static const char *EVENTPATH = "/var/lib/obmc/events";

static void load_existing_events (EventManager* em)
{
    vector<uint16_t> logids;
    vector<uint16_t>::iterator iter;
    uint16_t logid;
    Log *log;
    logids = em->logids();
    for (iter = logids.begin() ; iter != logids.end() ; iter++) {
        logid = *iter;
        if (em->open_log(logid, &log) == logid) {
            bus_on_create_log(log);
            em->close_log(log);
        }
        else {
            cerr << "WARN: fail to load log " << logid << endl;
        }
    }
}

static void print_usage (void)
{
    cerr << "[-s <x>] : Maximum bytes to use for event logger" << endl;
    cerr << "[-t <x>] : Maximum number of logs" << endl;
}

int main (int argc, char *argv[])
{
    size_t maxsize;
    uint16_t maxlogs;
    const char* sensor_type;
    const char* sensor_number;
    int c;
    int err;
    maxsize = 0;
    maxlogs = 0;
    sensor_type = "0x10"; // XXX hard-coded
    sensor_number = "0x80"; // XXX hard-coded
    while ((c = getopt(argc, argv, "s:t:")) != -1) {
        switch (c) {
            case 's':
                maxsize = (size_t) strtoul(optarg, NULL, 10);
                break;
            case 't':
                maxlogs = (uint16_t) strtoul(optarg, NULL, 10);
                break;
            case 'h':
            case '?':
                print_usage();
                return 1;
        }
    }
    EventManager em(EVENTPATH, maxsize, maxlogs,
            sensor_type, sensor_number,
            bus_on_create_log, bus_on_remove_log);
    if ((err = bus_build(&em)) == 0) {
        load_existing_events(&em);
        bus_mainloop();
        bus_cleaup();
    }
    return err;
}
