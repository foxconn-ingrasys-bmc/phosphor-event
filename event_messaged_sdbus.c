#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <systemd/sd-bus.h>
#include <time.h>
#include "event_messaged_sdbus.h"

struct MessageEntry {
    uint16_t logid;
    sd_bus_slot* slot_message;
    sd_bus_slot* slot_delete;
    sd_bus_slot* slot_associations;
    LIST_ENTRY(MessageEntry) entries;
};

static const char *EVENT_INTERFACE = "org.openbmc.recordlog";
static const char *EVENT_PATH = "/org/openbmc/records/events";

static sd_bus *sBus = NULL;
static sd_bus_slot *sSlot = NULL;
static EventManager *sEventManager = NULL;
static LIST_HEAD(MessageEntryHead, MessageEntry) sMessageEntries =
    LIST_HEAD_INITIALIZER();

static struct MessageEntry* message_entry_create (uint16_t logid)
{
    struct MessageEntry *msg;
    if ((msg = calloc(1, sizeof(struct MessageEntry)))) {
        LIST_INSERT_HEAD(&sMessageEntries, msg, entries);
        msg->logid = logid;
        return msg;
    }
    else {
        return NULL;
    }
}

static void message_entry_delete (struct MessageEntry* msg)
{
    LIST_REMOVE(msg, entries);
    free(msg);
}

static struct MessageEntry* message_entry_find (uint16_t logid)
{
    struct MessageEntry *msg;
    LIST_FOREACH(msg, &sMessageEntries, entries) {
        if (msg->logid == logid) {
            return msg;
        }
    }
    return NULL;
}

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
    err = sd_bus_message_read(bm, "sssss",
            &log.severity,
            &log.message,
            &log.sensor_type,
            &log.sensor_number,
            &log.association);
    if (err < 0) {
        fprintf(stderr, "ERR: fail to record BMC log: %s\n", strerror(-err));
        return err;
    }
    if (!(strncmp(log.severity, "DEBUG", 6) == 0 ||
                strncmp(log.severity, "INFO", 5) == 0 ||
                strncmp(log.severity, "ERROR", 6) == 0)) {
        fprintf(stderr,
                "ERR: fail to record BMC log: invalid severity \"%s\"\n",
                log.severity);
        return -1;
    }
    err = sd_bus_message_read_array(bm, 'y',
            (const void **) &log.debug_data,
            &log.debug_data_len);
    if (err < 0) {
        fprintf(stderr, "ERR: fail to record BMC log: %s\n", strerror(-err));
        return err;
    }
    log.reporter = "BMC";
    if ((logid = message_log_create(sEventManager, &log)) != 0) {
        fprintf(stderr, "INFO: record BMC log %d: %s\n", logid, log.message);
        return sd_bus_reply_method_return(bm, "q", logid);
    }
    else {
        fprintf(stderr, "ERR: fail to record BMC log\n");
    }
    return 0;
}

static int method_clear_all (
        sd_bus_message* bm,
        void* user_data,
        sd_bus_error* error)
{
    uint16_t logid;
    if ((logid = message_log_clear_all(sEventManager)) != 0) {
        fprintf(stderr, "INFO: clear all logs\n");
        return sd_bus_reply_method_return(bm, "q", logid);
    }
    else {
        fprintf(stderr, "ERR: fail to record BMC log\n");
        return sd_bus_reply_method_return(bm, "q", 0);
    }
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
            "Not enough memory for all log IDs");
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
    SD_BUS_METHOD("acceptBMCMessage", "sssssay", "q",
            method_accept_bmc, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("clear", NULL, "q",
            method_clear_all, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getAllLogIds", NULL, "aq",
            method_get_all_logids, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static int property_message (
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* bm,
        void* user_data, sd_bus_error* error)
{
    struct MessageEntry *msg;
    Log *log;
    char buffer[32];
    int err;
    msg = user_data;
    if (message_log_open(sEventManager, msg->logid, &log) != msg->logid) {
        fprintf(stderr, "ERR: fail to read property %s of log %d: %s\n",
                property, msg->logid, "can not open log file");
        sd_bus_error_set(error, SD_BUS_ERROR_FILE_NOT_FOUND,
            "Could not find log file");
        return -1;
    }
    if (!strncmp("message", property, 7)) {
        err = sd_bus_message_append(bm, "s", log->message);
    }
    else if (!strncmp("severity", property, 8)) {
        err = sd_bus_message_append(bm, "s", log->severity);
    }
    else if (!strncmp("sensor_type", property, 11)) {
        err = sd_bus_message_append(bm, "s", log->sensor_type);
    }
    else if (!strncmp("sensor_number", property, 13)) {
        err = sd_bus_message_append(bm, "s", log->sensor_number);
    }
    else if (!strncmp("reported_by", property, 11)) {
        err = sd_bus_message_append(bm, "s", log->reporter);
    }
    else if (!strncmp("time", property, 4)) {
        strftime(buffer, 32, "%Y:%m:%d %H:%M:%S",
                localtime(&log->timestamp.tv_sec));
        err = sd_bus_message_append(bm, "s", buffer);
    }
    else {
        err = sd_bus_message_append(bm, "s", "");
    }
    if (err < 0) {
        fprintf(stderr, "ERR: fail to read property %s of log %d: %s\n",
                property, msg->logid, strerror(-err));
    }
    message_log_close(sEventManager, log);
    return err;
}

static int property_debug_data (
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* bm,
        void* user_data, sd_bus_error* error)
{
    struct MessageEntry *msg;
    Log *log;
    int err;
    msg = user_data;
    if (message_log_open(sEventManager, msg->logid, &log) != msg->logid) {
        fprintf(stderr, "ERR: fail to read property %s of log %d: %s\n",
                property, msg->logid, "can not open log file");
        sd_bus_error_set(error, SD_BUS_ERROR_FILE_NOT_FOUND,
                "Could not find log file");
        return -1;
    }
    err = sd_bus_message_append_array(bm, 'y',
            log->debug_data, log->debug_data_len);
    if (err < 0) {
        fprintf(stderr, "ERR: fail to read property %s of log %d: %s\n",
                property, msg->logid, strerror(-err));
    }
    message_log_close(sEventManager, log);
    return err;
}

static const sd_bus_vtable TABLE_PROPERTIES[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("message", "s",
            property_message, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("severity", "s",
            property_message, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("sensor_type", "s",
            property_message, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("sensor_number", "s",
            property_message, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("reported_by", "s",
            property_message, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("time", "s",
            property_message, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("debug_data", "ay",
            property_debug_data, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_VTABLE_END
};

static int method_delete (
        sd_bus_message* bm,
        void* user_data,
        sd_bus_error* error)
{
    struct MessageEntry *msg;
    uint16_t logid;
    msg = user_data;
    logid = msg->logid;
    message_log_delete(sEventManager, msg->logid);
    fprintf(stderr, "INFO: delete log %d\n", logid);
    return sd_bus_reply_method_return(bm, "q", 0);
}

static const sd_bus_vtable TABLE_DELETE[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("delete", NULL, "q",
            method_delete, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static int property_associations (
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* bm,
        void* user_data, sd_bus_error* error)
{
    struct MessageEntry *msg;
    Log *log;
    int err;
    char *p;
    char *token;
    msg = user_data;
    if (message_log_open(sEventManager, msg->logid, &log) != msg->logid) {
        fprintf(stderr, "ERR: fail to read property %s of log %d: %s\n",
                property, msg->logid, "can not open log file");
        sd_bus_error_set(error, SD_BUS_ERROR_FILE_NOT_FOUND,
            "Could not find log file");
        return -1;
    }
    p = strdup(log->association);
    message_log_close(sEventManager, log);
    if (!p) {
        fprintf(stderr, "ERR: fail to read property %s of log %d: %s\n",
                property, msg->logid, "out of memory");
        sd_bus_error_set(error, SD_BUS_ERROR_NO_MEMORY,
            "Not enough memory for association");
        return -1;
    }
    err = 0;
    token = strtok(p, " ");
    if (token) {
        err = sd_bus_message_open_container(bm, 'a', "(sss)");
        while (token) {
            err = sd_bus_message_append(bm, "(sss)", "fru", "event", token);
            token = strtok(NULL, " ");
        }
        err = sd_bus_message_close_container(bm);
    }
    if (err < 0) {
        fprintf(stderr, "ERR: fail to get associations of log %d: %s\n",
                msg->logid, strerror(-err));
    }
    free(p);
    return err;
}

static const sd_bus_vtable TABLE_ASSOCIATIONS[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("associations", "a(sss)",
            property_associations, 0, SD_BUS_VTABLE_PROPERTY_CONST),
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

void bus_on_create_log (const Log* log)
{
    char object_path[64];
    struct MessageEntry *msg;
    int err;
    if (!(msg = message_entry_create(log->logid))) {
        fprintf(stderr, "ERR: fail to create log: %s\n", "out of memory");
        return;
    }
    message_entry_get_path(object_path, log->logid);
    err = sd_bus_add_object_vtable(sBus, &msg->slot_message,
            object_path, "org.openbmc.record",
            TABLE_PROPERTIES, msg);
    if (err < 0) {
        goto ERR;
    }
    err = sd_bus_add_object_vtable(sBus, &msg->slot_delete,
            object_path, "org.openbmc.Object.Delete",
            TABLE_DELETE, msg);
    if (err < 0) {
        goto ERR;
    }
    msg->slot_associations = NULL;
    if (0 < strlen(log->association)) {
        err = sd_bus_add_object_vtable(sBus, &msg->slot_associations,
                object_path, "org.openbmc.Associations",
                TABLE_ASSOCIATIONS, msg);
        if (err < 0) {
            goto ERR;
        }
    }
    return;
ERR:
    fprintf(stderr, "ERR: fail to create log: %s\n", strerror(-err));
    if (msg->slot_message) {
        sd_bus_slot_unref(msg->slot_message);
    }
    if (msg->slot_delete) {
        sd_bus_slot_unref(msg->slot_delete);
    }
    if (msg->slot_associations) {
        sd_bus_slot_unref(msg->slot_associations);
    }
    message_entry_delete(msg);
}

void bus_on_remove_log (const Log* log)
{
    struct MessageEntry *msg;
    char object_path[64];
    if ((msg = message_entry_find(log->logid))) {
        message_entry_get_path(object_path, msg->logid);
        sd_bus_slot_unref(msg->slot_message);
        sd_bus_slot_unref(msg->slot_delete);
        if (msg->slot_associations) {
            sd_bus_slot_unref(msg->slot_associations);
        }
        message_entry_delete(msg);
    }
}
