#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <time.h>
#include "event_messaged_sdbus.h"

static const char *BUS_NAME = "org.openbmc.records.events";
static const char *BUS_PATH = "/org/openbmc/records/events";
static const char *BUS_INTERFACE = "org.openbmc.recordlog";

static sd_bus *sBus = NULL;
static sd_bus_slot *sSlot = NULL;
static EventManager *sEventManager = NULL;

static int method_create__read_string (
        const char* field_name,
        sd_bus_message* bm,
        size_t max_length,
        char* string)
{
    int err;
    const char *buff;
    if ((err = sd_bus_message_read(bm, "s", &buff)) < 0) {
        fprintf(stderr, "ERR: failed to create log: %s: %s\n",
                field_name, strerror(-err));
        return -1;
    }
    if (strlen(buff) < max_length) {
        strcpy(string, buff);
        return 0;
    }
    else {
        fprintf(stderr, "ERR: failed to create log: %s too long\n", field_name);
        return -2;
    }
}

static int method_create (
        sd_bus_message* bm,
        void* user_data,
        sd_bus_error* error)
{
    int err;
    Log log = {0};
    uint16_t record_id;
    if ((err = method_create__read_string("severity", bm,
                    LOG_SEVERITY_MAX_LENGTH, log.severity)) != 0) {
        return err;
    }
    if ((err = method_create__read_string("entry_type", bm,
                    LOG_ENTRY_TYPE_MAX_LENGTH, log.entry_type)) != 0) {
        return err;
    }
    if ((err = method_create__read_string("entry_code", bm,
                    LOG_ENTRY_CODE_MAX_LENGTH, log.entry_code)) != 0) {
        return err;
    }
    if ((err = method_create__read_string("sensor_type", bm,
                    LOG_SENSOR_TYPE_MAX_LENGTH, log.sensor_type)) != 0) {
        return err;
    }
    if ((err = method_create__read_string("sensor_number", bm,
                    LOG_SENSOR_NUMBER_MAX_LENGTH, log.sensor_number)) != 0) {
        return err;
    }
    if ((err = method_create__read_string("message", bm,
                    LOG_MESSAGE_MAX_LENGTH, log.message)) != 0) {
        return err;
    }
    if ((err = method_create__read_string("raw_data", bm,
                    LOG_RAW_DATA_MAX_LENGTH, log.raw_data)) != 0) {
        return err;
    }
    if ((record_id = message_create(sEventManager, &log)) != 0) {
        fprintf(stderr, "INFO: created log %u\n", record_id);
    }
    else {
        fprintf(stderr, "ERR: failed to create log\n");
    }
    return sd_bus_reply_method_return(bm, "q", record_id);
}

static int method_clear (
        sd_bus_message* bm,
        void* user_data,
        sd_bus_error* error)
{
    int err;
    uint8_t sensor_number;
    uint16_t record_id;
    if ((err = sd_bus_message_read(bm, "y", &sensor_number)) < 0) {
        fprintf(stderr, "ERR: failed to clear all logs: %s\n", strerror(-err));
        return err;
    }
    if ((record_id = message_clear_all(sEventManager, sensor_number)) != 0) {
        fprintf(stderr, "INFO: cleared all logs\n");
    }
    else {
        fprintf(stderr, "ERR: failed to record log\n");
    }
    return sd_bus_reply_method_return(bm, "q", record_id);
}

static int method_get_record_ids_and_logical_timestamps (
        sd_bus_message* bm,
        void* user_data,
        sd_bus_error* error)
{
    uint16_t *record_ids;
    uint64_t *timestamps;
    uint16_t count;
    int err;
    sd_bus_message *reply;
    if (message_get_record_ids_and_logical_timestamps(
                sEventManager,
                &record_ids,
                &timestamps,
                &count) != 0) {
        return -1;
    }
    if ((err = sd_bus_message_new_method_return(bm, &reply)) < 0) {
        fprintf(stderr, "ERR: failed to get record IDs and timestamps: %s\n",
                strerror(-err));
        free(record_ids);
        free(timestamps);
        return err;
    }
    err = sd_bus_message_append_array(reply, SD_BUS_TYPE_UINT16,
            record_ids, count * sizeof(uint16_t));
    if (err < 0) {
        fprintf(stderr, "ERR: failed to get record IDs and timestamps: %s\n",
                strerror(-err));
        free(record_ids);
        free(timestamps);
        return err;
    }
    err = sd_bus_message_append_array(reply, SD_BUS_TYPE_UINT64,
            timestamps, count * sizeof(uint64_t));
    if (err < 0) {
        fprintf(stderr, "ERR: failed to get record IDs and timestamps: %s\n",
                strerror(-err));
        free(record_ids);
        free(timestamps);
        return err;
    }
    err = sd_bus_send(sd_bus_message_get_bus(bm), reply, NULL);
    if (err < 0) {
        fprintf(stderr, "ERR: failed to get all log ids: %s\n", strerror(-err));
        free(record_ids);
        free(timestamps);
        return err;
    }
    free(record_ids);
    free(timestamps);
    return 0;
}

static int method_get_rollover_count (
        sd_bus_message* bm,
        void* user_data,
        sd_bus_error* error)
{
    uint8_t rollover_count;
    rollover_count = message_get_rollover_count(sEventManager);
    return sd_bus_reply_method_return(bm, "y", rollover_count);
}

static const sd_bus_vtable TABLE_EVENT[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("create", "sssssss", "q",
            method_create, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("clear", "y", "q",
            method_clear, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("get_record_ids_and_logical_timestamps", NULL, "aqat",
            method_get_record_ids_and_logical_timestamps,
            SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("get_rollover_count", NULL, "y",
            method_get_rollover_count, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

int bus_build (EventManager* em)
{
    int err;
    sEventManager = em;
    err = sd_bus_open_system(&sBus);
    if (err < 0) {
        fprintf(stderr, "ERR: failed to open system bus: %s\n",
                strerror(-err));
        goto OUT;
    }
    err = sd_bus_add_object_vtable(sBus, &sSlot,
            BUS_PATH, BUS_INTERFACE,
            TABLE_EVENT, sEventManager);
    if (err < 0) {
        fprintf(stderr, "ERR: failed to register object to bus: %s\n",
                strerror(-err));
        goto OUT;
    }
    err = sd_bus_request_name(sBus, BUS_NAME, 0);
    if (err < 0) {
        fprintf(stderr, "ERR: failed to request bus name: %s\n",
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
                fprintf(stderr, "ERR: failed to wait for bus: %s\n",
                        strerror(-err));
                break;
            }
        }
        else {
            fprintf(stderr, "ERR: failed to process bus: %s\n",
                    strerror(-err));
            break;
        }
    }
}
