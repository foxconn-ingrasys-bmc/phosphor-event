#include <assert.h>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/time.h>
#include <systemd/sd-bus.h>
#include <unistd.h>
#include <vector>
#include "message.H"

using namespace std;

const string EVENTSPATH = "./events";

static void test_suite_build_log (
        Log* log,
        const char* message,
        const char* severity,
        const char* sensor_type,
        const char* sensor_number,
        const char* association,
        const char* reporter,
        uint8_t* debug_data,
        size_t debug_data_len)
{
    log->message = (char*) message;
    log->severity = (char*) severity;
    log->sensor_type = (char*) sensor_type;
    log->sensor_number = (char*) sensor_number;
    log->association = (char*) association;
    log->reporter = (char*) reporter;
    log->debug_data = debug_data;
    log->debug_data_len = debug_data_len;
}

static void test_suite_setup (void)
{
    char *cmd;
    asprintf(&cmd, "exec rm -r %s 2> /dev/null", EVENTSPATH.c_str());
    system(cmd);
    free(cmd);
    asprintf(&cmd, "exec mkdir %s 2> /dev/null", EVENTSPATH.c_str());
    system(cmd);
    free(cmd);
}

static void test_suite (void)
{
    uint8_t P[] = {0x3, 0x32, 0x34, 0x36};
    Log REC1;
    Log REC2;
    vector<uint16_t> logids;

    test_suite_build_log(&REC1, "Testing Message1", "DEBUG", "0xCD",
            "0xAB", "Association", "Test", P, 4);
    test_suite_build_log(&REC2, "Testing Message2", "DEBUG", "0xCD",
            "0xAB", "Association", "Test", P, 4);

    /* TEST: build 2 logs and verify their content. */
    Log *log;
    test_suite_setup();
    EventManager em0(EVENTSPATH, 0, 0);
    assert(em0.create_log(&REC1) == 1);
    assert(em0.managed_size() == 86);
    assert(em0.managed_count() == 1);
    assert(em0.latest_logid() == 1);
    assert(em0.create_log(&REC2) == 2);
    assert(em0.managed_size() == 172);
    assert(em0.managed_count() == 2);
    assert(em0.latest_logid() == 2);
    assert(em0.open_log(1, &log) == 1);
    assert(strcmp(log->message, "Testing Message1") == 0);
    assert(strcmp(log->severity, "DEBUG") == 0);
    assert(strcmp(log->sensor_type, "0xCD") == 0);
    assert(strcmp(log->sensor_number, "0xAB") == 0);
    assert(strcmp(log->association, "Association") == 0);
    assert(strcmp(log->reporter, "Test") == 0);
    assert(log->debug_data_len == 4);;
    assert(log->debug_data[0] == 0x3);
    assert(log->debug_data[1] == 0x32);
    assert(log->debug_data[2] == 0x34);
    assert(log->debug_data[3] == 0x36);
    assert(log->logid == 1);
    em0.close_log(log);
    assert(em0.open_log(2, &log) == 2);
    assert(strcmp(log->message, "Testing Message2") == 0);
    em0.close_log(log);

    /* TEST: log count, ID, and size should persist across event manager. */
    EventManager em1(EVENTSPATH, 0, 0);
    assert(em1.managed_size() == 172);
    assert(em1.managed_count() == 2);
    assert(em1.latest_logid() == 2);

    /* TEST: max limits. */
    test_suite_setup();
    EventManager em2(EVENTSPATH, 85, 0);
    assert(em2.create_log(&REC1) == 0);

    /* TEST: next log ID is derived from the latest log. */
    test_suite_setup();
    EventManager em3(EVENTSPATH, 0, 4);
    assert(em3.create_log(&REC1) == 1);
    assert(em3.create_log(&REC1) == 2);
    assert(em3.create_log(&REC1) == 3);
    em3.remove_log(2);
    assert(em3.create_log(&REC1) == 4);
    assert(em3.create_log(&REC1) == 2);

    /* TEST: circular event log. */
    test_suite_setup();
    EventManager em4(EVENTSPATH, 0, 4);
    assert(em4.create_log(&REC1) == 1);
    assert(em4.create_log(&REC1) == 2);
    assert(em4.create_log(&REC1) == 3);
    assert(em4.create_log(&REC1) == 4);
    assert(em4.create_log(&REC1) == 1);
    assert(em4.create_log(&REC1) == 2);
    assert(em4.create_log(&REC1) == 3);
    assert(em4.create_log(&REC1) == 4);
    assert(em4.create_log(&REC1) == 1);
    assert(em4.create_log(&REC1) == 2);
    assert(em4.create_log(&REC1) == 3);
    assert(em4.create_log(&REC1) == 4);

    /* TEST: circular event log and random deletion. */
    test_suite_setup();
    EventManager em5(EVENTSPATH, 0, 4);
    assert(em5.create_log(&REC1) == 1);
    assert(em5.create_log(&REC1) == 2);
    assert(em5.create_log(&REC1) == 3);
    assert(em5.create_log(&REC1) == 4);
    logids = em5.logids();
    assert(logids.size() == 4);
    assert(logids[0] == 1);
    assert(logids[1] == 2);
    assert(logids[2] == 3);
    assert(logids[3] == 4);
    em5.remove_log(2);
    em5.remove_log(3);
    logids = em5.logids();
    assert(logids.size() == 2);
    assert(logids[0] == 1);
    assert(logids[1] == 4);
    assert(em5.create_log(&REC1) == 2);
    assert(em5.create_log(&REC1) == 3);
    assert(em5.create_log(&REC1) == 1);
    assert(em5.create_log(&REC1) == 4);
    logids = em5.logids();
    assert(logids.size() == 4);
    assert(logids[0] == 2);
    assert(logids[1] == 3);
    assert(logids[2] == 1);
    assert(logids[3] == 4);

    /* TEST: individual removal. */
    test_suite_setup();
    EventManager em6(EVENTSPATH, 0, 3);
    assert(em6.create_log(&REC1) == 1);
    assert(em6.create_log(&REC1) == 2);
    assert(em6.create_log(&REC1) == 3);
    em6.remove_log(2);
    assert(em6.managed_count() == 2);
    em6.remove_log(3);
    assert(em6.managed_count() == 1);
    em6.remove_log(1);
    assert(em6.managed_count() == 0);
    em6.remove_log(1);

    /* TEST: remove all. */
    test_suite_setup();
    EventManager em7(EVENTSPATH, 0, 0);
    assert(em7.create_log(&REC1) == 1);
    assert(em7.create_log(&REC1) == 2);
    assert(em7.create_log(&REC1) == 3);
    em7.remove_all_logs();
    assert(em7.managed_count() == 0);
}

static void test_suite_systemd (void)
{
    const uint8_t debug_data[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    sd_bus *bus = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *req = NULL;
    sd_bus_message *res = NULL;
    uint16_t logid1;
    uint16_t logid2;
    char object_path[64];
    char *property;
    const uint8_t *prop_debug_data;
    size_t debug_data_len;
    char log_path[64];
    const uint16_t *logids;
    size_t logid_count;

    /* SETUP SD-BUS */
    assert(0 <= sd_bus_open_system(&bus));

    /* TEST: method acceptBMCMessage */
    assert(0 <= sd_bus_message_new_method_call(
                bus,
                &req,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage"));
    assert(0 <= sd_bus_message_append(
                req,
                "sssss",
                "DEBUG",
                "Message",
                "0xCD",
                "0xAB",
                "Association"));
    assert(0 <= sd_bus_message_append_array(
                req,
                'y',
                debug_data,
                4));
    assert(0 <= sd_bus_call(bus, req, 0, &error, &res));
    assert(0 <= sd_bus_message_read(res, "q", &logid1));
    sd_bus_message_unref(req);
    sd_bus_message_unref(res);
    assert(logid1 != 0);

    /* TEST: method acceptBMCMessage should accept limited severity */
    assert(0 <= sd_bus_message_new_method_call(
                bus,
                &req,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage"));
    assert(0 <= sd_bus_message_append(
                req,
                "sssss",
                "INVALID_SEVERITY",
                "Message",
                "0xCD",
                "0xAB",
                "Association"));
    assert(0 <= sd_bus_message_append_array(
                req,
                'y',
                debug_data,
                4));
    assert(sd_bus_call(bus, req, 0, &error, &res) < 0);
    sd_bus_message_unref(req);
    sd_bus_error_free(&error);

    /* TEST: property message */
    snprintf(object_path, 64, "/org/openbmc/records/events/%d", logid1);
    assert(0 <= sd_bus_get_property_string(
                bus,
                "org.openbmc.records.events",
                object_path,
                "org.openbmc.record",
                "message",
                &error,
                &property));
    assert(strcmp(property, "Message") == 0);
    free(property);

    /* TEST: property severity */
    snprintf(object_path, 64, "/org/openbmc/records/events/%d", logid1);
    assert(0 <= sd_bus_get_property_string(
                bus,
                "org.openbmc.records.events",
                object_path,
                "org.openbmc.record",
                "severity",
                &error,
                &property));
    assert(strcmp(property, "DEBUG") == 0);
    free(property);

    /* TEST: property sensor_type */
    snprintf(object_path, 64, "/org/openbmc/records/events/%d", logid1);
    assert(0 <= sd_bus_get_property_string(
                bus,
                "org.openbmc.records.events",
                object_path,
                "org.openbmc.record",
                "sensor_type",
                &error,
                &property));
    assert(strcmp(property, "0xCD") == 0);
    free(property);

    /* TEST: property sensor_number */
    snprintf(object_path, 64, "/org/openbmc/records/events/%d", logid1);
    assert(0 <= sd_bus_get_property_string(
                bus,
                "org.openbmc.records.events",
                object_path,
                "org.openbmc.record",
                "sensor_number",
                &error,
                &property));
    assert(strcmp(property, "0xAB") == 0);
    free(property);

    /* TEST: property reported_by */
    snprintf(object_path, 64, "/org/openbmc/records/events/%d", logid1);
    assert(0 <= sd_bus_get_property_string(
                bus,
                "org.openbmc.records.events",
                object_path,
                "org.openbmc.record",
                "reported_by",
                &error,
                &property));
    assert(strcmp(property, "BMC") == 0);
    free(property);

    /* TEST: property time */
    snprintf(object_path, 64, "/org/openbmc/records/events/%d", logid1);
    assert(0 <= sd_bus_get_property_string(
                bus,
                "org.openbmc.records.events",
                object_path,
                "org.openbmc.record",
                "time",
                &error,
                &property));
    assert(0 < strlen(property));
    free(property);

    /* TEST: property debug data */
    snprintf(object_path, 64, "/org/openbmc/records/events/%d", logid1);
    assert(0 <= sd_bus_get_property(
                bus,
                "org.openbmc.records.events",
                object_path,
                "org.openbmc.record",
                "debug_data",
                &error,
                &res,
                "ay"));
    assert(0 <= sd_bus_message_read_array(res, SD_BUS_TYPE_BYTE,
                (const void**) &prop_debug_data, &debug_data_len));
    assert(debug_data_len == 4);
    assert(memcmp(prop_debug_data, debug_data, 4) == 0);
    sd_bus_message_unref(res);

    /* TEST: method event.delete */
    snprintf(object_path, 64, "/org/openbmc/records/events/%d", logid1);
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                object_path,
                "org.openbmc.Object.Delete",
                "delete",
                &error,
                &res,
                NULL));
    sd_bus_message_unref(res);
    snprintf(log_path, 64, "/var/lib/obmc/events/%d", logid1);
    assert(access(log_path, F_OK) != 0);

    /* TEST: method clear */
    assert(0 <= sd_bus_message_new_method_call(
                bus,
                &req,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage"));
    assert(0 <= sd_bus_message_append(
                req,
                "sssss",
                "DEBUG",
                "Message",
                "0xCD",
                "0xAB",
                "Association"));
    assert(0 <= sd_bus_message_append_array(
                req,
                'y',
                debug_data,
                4));
    assert(0 <= sd_bus_call(bus, req, 0, &error, &res));
    assert(0 <= sd_bus_message_read(res, "q", &logid1));
    sd_bus_message_unref(req);
    sd_bus_message_unref(res);
    assert(logid1 != 0);
    assert(0 <= sd_bus_message_new_method_call(
                bus,
                &req,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage"));
    assert(0 <= sd_bus_message_append(
                req,
                "sssss",
                "DEBUG",
                "Message",
                "0xCD",
                "0xAB",
                "Association"));
    assert(0 <= sd_bus_message_append_array(
                req,
                'y',
                debug_data,
                4));
    assert(0 <= sd_bus_call(bus, req, 0, &error, &res));
    assert(0 <= sd_bus_message_read(res, "q", &logid2));
    sd_bus_message_unref(req);
    sd_bus_message_unref(res);
    assert(logid2 != 0);
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "clear",
                &error,
                &res,
                NULL));
    sd_bus_message_unref(res);
    snprintf(log_path, 64, "/var/lib/obmc/events/%d", logid2);
    assert(access(log_path, F_OK) != 0);
    assert(0 <= sd_bus_get_property_string(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events/1",
                "org.openbmc.record",
                "sensor_type",
                &error,
                &property));
    assert(strcmp(property, "0x10") == 0);
    free(property);
    assert(0 <= sd_bus_get_property_string(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events/1",
                "org.openbmc.record",
                "sensor_number",
                &error,
                &property));
    assert(strcmp(property, "0x80") == 0);
    free(property);
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "getAllLogIds",
                &error,
                &res,
                NULL));
    assert(0 <= sd_bus_message_read_array(res, SD_BUS_TYPE_UINT16,
                (const void**) &logids, &logid_count));
    logid_count /= sizeof(uint16_t);
    assert(logid_count == 1);
    sd_bus_message_unref(res);

    /* TEST: method getAllLogIds */
    assert(0 <= sd_bus_message_new_method_call(
                bus,
                &req,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage"));
    assert(0 <= sd_bus_message_append(
                req,
                "sssss",
                "DEBUG",
                "Message",
                "0xCD",
                "0xAB",
                "Association"));
    assert(0 <= sd_bus_message_append_array(
                req,
                'y',
                debug_data,
                4));
    assert(0 <= sd_bus_call(bus, req, 0, &error, &res));
    assert(0 <= sd_bus_message_read(res, "q", &logid1));
    sd_bus_message_unref(req);
    sd_bus_message_unref(res);
    assert(logid1 != 0);
    assert(0 <= sd_bus_message_new_method_call(
                bus,
                &req,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage"));
    assert(0 <= sd_bus_message_append(
                req,
                "sssss",
                "DEBUG",
                "Message",
                "0xCD",
                "0xAB",
                "Association"));
    assert(0 <= sd_bus_message_append_array(
                req,
                'y',
                debug_data,
                4));
    assert(0 <= sd_bus_call(bus, req, 0, &error, &res));
    assert(0 <= sd_bus_message_read(res, "q", &logid2));
    sd_bus_message_unref(req);
    sd_bus_message_unref(res);
    assert(logid2 != 0);
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "getAllLogIds",
                &error,
                &res,
                NULL));
    assert(0 <= sd_bus_message_read_array(res, SD_BUS_TYPE_UINT16,
                (const void**) &logids, &logid_count));
    logid_count /= sizeof(uint16_t);
    assert(logid_count == 3); // the first log is "clearing all logs"
    assert(logids[1] == logid1);
    assert(logids[2] == logid2);
    sd_bus_message_unref(res);

    /* TEARDOWN SD-BUS */
    sd_bus_unref(bus);
}

int main(int argc, char *argv[])
{
    struct timeval begin;
    struct timeval end;
    uint64_t sec;
    uint64_t usec;
    gettimeofday(&begin, NULL);
    if (argc == 1) {
        test_suite();
    }
    else if (2 <= argc) {
        test_suite_systemd();
    }
    gettimeofday(&end, NULL);
    usec = (end.tv_sec * 1000000 + end.tv_usec) -
        (begin.tv_sec * 1000000 + begin.tv_usec);
    sec = usec / 1000000;
    usec = usec % 1000000;
    printf("Test takes %llu seconds %llu microseconds\n", sec, usec);
    return 0;
}
