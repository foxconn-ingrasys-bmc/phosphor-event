#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include "event_messaged_sdbus.h"

using namespace std;

static const char *EVENTPATH = "/var/lib/obmc/events";

static void print_usage (void)
{
    cerr << "[-s <x>] : Maximum bytes to use for event logger" << endl;
    cerr << "[-t <x>] : Maximum number of logs" << endl;
}

int main (int argc, char *argv[])
{
    size_t maxsize;
    uint16_t maxlogs;
    int c;
    int err;
    maxsize = 0;
    maxlogs = 0;
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
    EventManager em(EVENTPATH, maxsize, maxlogs);
    if ((err = bus_build(&em)) == 0) {
        bus_mainloop();
        bus_cleaup();
    }
    return err;
}
