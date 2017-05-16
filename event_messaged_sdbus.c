#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <time.h>
#include "event_messaged_sdbus.h"

static const char *EVENT_INTERFACE = "org.openbmc.recordlog";
static const char *EVENT_PATH = "/org/openbmc/records/events";

static sd_bus *sBus = NULL;
static sd_bus_slot *sSlot = NULL;
static EventManager *sEventManager = NULL;

static void message_entry_get_path (char object_path[64], uint16_t logid)
{
    int n;
    n = snprintf(object_path, 64, "%s/%d", EVENT_PATH, logid);
    if (64 <= n) {
        fprintf(stderr, "WARN: object path is too long\n");
    }
}

static int method_accept_bmc (
        sd_bus_message* bm,
        void* user_data,
        sd_bus_error* error)
{
    int err;
    uint16_t logid;
    Log log = {0};
    err = sd_bus_message_read(bm, "yyyyyyy",
            &log.severity,
            &log.sensor_type,
            &log.sensor_number,
            &log.event_dir_type,
            &log.event_data[0],
            &log.event_data[1],
            &log.event_data[2]);
    if (err < 0) {
        fprintf(stderr, "ERR: fail to record log: %s\n", strerror(-err));
        return err;
    }
    if (!(log.severity == SEVERITY_INFO ||
                log.severity == SEVERITY_WARN ||
                log.severity == SEVERITY_CRIT)) {
        fprintf(stderr,
                "ERR: fail to record log: invalid severity \"%d\"\n",
                log.severity);
        return -1;
    }
    if ((logid = message_log_create(sEventManager, &log)) != 0) {
        fprintf(stderr, "INFO: record log %d\n", logid);
        return sd_bus_reply_method_return(bm, "q", logid);
    }
    else {
        fprintf(stderr, "ERR: fail to record log\n");
    }
    return 0;
}

static int method_clear_all (
        sd_bus_message* bm,
        void* user_data,
        sd_bus_error* error)
{
    int err;
    uint8_t sensor_number;
    uint16_t logid;
    if ((err = sd_bus_message_read(bm, "y", &sensor_number)) < 0) {
        fprintf(stderr, "ERR: fail to clear all logs: %s\n", strerror(-err));
        return err;
    }
    if ((logid = message_log_clear_all(sEventManager, sensor_number)) != 0) {
        fprintf(stderr, "INFO: clear all logs\n");
        return sd_bus_reply_method_return(bm, "q", logid);
    }
    else {
        fprintf(stderr, "ERR: fail to record log\n");
        return sd_bus_reply_method_return(bm, "q", 0);
    }
}

static int method_delete (
        sd_bus_message* bm,
        void* user_data,
        sd_bus_error* error)
{
    int err;
    uint16_t logid;
    err = sd_bus_message_read(bm, "q", &logid);
    if (err < 0) {
        fprintf(stderr, "ERR: fail to delete log: %s\n", strerror(-err));
        return err;
    }
    message_log_delete(sEventManager, logid);
    return sd_bus_reply_method_return(bm, "q", 0);
}

static int method_get_all_logids (
        sd_bus_message* bm,
        void* user_data,
        sd_bus_error* error)
{
    uint16_t *logids;
    uint16_t count;
    int err;
    sd_bus_message *reply;
    if (message_log_get_all_logids(sEventManager, &logids, &count) != 0) {
        sd_bus_error_set(error, SD_BUS_ERROR_NO_MEMORY,
            "insufficient memory for all log IDs");
        return -1;
    }
    if ((err = sd_bus_message_new_method_return(bm, &reply)) < 0) {
        fprintf(stderr, "ERR: fail to get all log ids: %s\n", strerror(-err));
        free(logids);
        return err;
    }
    err = sd_bus_message_append_array(reply, SD_BUS_TYPE_UINT16,
            logids, count * sizeof(uint16_t));
    if (err < 0) {
        fprintf(stderr, "ERR: fail to get all log ids: %s\n", strerror(-err));
        free(logids);
        return err;
    }
    err = sd_bus_send(sd_bus_message_get_bus(bm), reply, NULL);
    if (err < 0) {
        fprintf(stderr, "ERR: fail to get all log ids: %s\n", strerror(-err));
    }
    free(logids);
    return err;
}

static const sd_bus_vtable TABLE_EVENT[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("acceptBMCMessage", "yyyyyyy", "q",
            method_accept_bmc, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("clear", "y", "q",
            method_clear_all, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("delete", "q", "q",
            method_delete, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getAllLogIds", NULL, "aq",
            method_get_all_logids, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

int bus_build (EventManager* em)
{
    int err;
    sEventManager = em;
    err = sd_bus_open_system(&sBus);
    if (err < 0) {
        fprintf(stderr, "ERR: fail to open system bus: %s\n",
                strerror(-err));
        goto OUT;
    }
    err = sd_bus_add_object_vtable(sBus, &sSlot,
            EVENT_PATH, EVENT_INTERFACE,
            TABLE_EVENT, sEventManager);
    if (err < 0) {
        fprintf(stderr, "ERR: fail to register object to bus: %s\n",
                strerror(-err));
        goto OUT;
    }
    err = sd_bus_request_name(sBus, "org.openbmc.records.events", 0);
    if (err < 0) {
        fprintf(stderr, "ERR: fail to request bus name: %s\n",
                strerror(-err));
        goto OUT;
    }
OUT:
    if (0 <= err) {
        fprintf(stderr, "DEBUG: opened system bus\n");
        return 0;
    }
    else {
        bus_cleaup();
        return err;
    }
}

void bus_cleaup (void)
{
    sd_bus_slot_unref(sSlot);
    sd_bus_unref(sBus);
}

void bus_mainloop (void)
{
    int err;
    while (1) {
        err = sd_bus_process(sBus, NULL);
        if (0 < err) {
            continue;
        }
        else if (err == 0) {
            if ((err = sd_bus_wait(sBus, -1)) < 0) {
                fprintf(stderr, "ERR: fail to wait for bus: %s\n",
                        strerror(-err));
                break;
            }
        }
        else {
            fprintf(stderr, "ERR: fail to process bus: %s\n",
                    strerror(-err));
            break;
        }
    }
}
