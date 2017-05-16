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
        uint8_t severity,
        uint8_t sensor_type,
        uint8_t sensor_number,
        uint8_t event_dir_type,
        uint8_t event_data[3])
{
    log->severity = severity;
    log->sensor_type = sensor_type;
    log->sensor_number = sensor_number;
    log->event_dir_type = event_dir_type;
    memcpy(log->event_data, event_data, sizeof(event_data));
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
    uint8_t P[3] = {0x3, 0x32, 0x34};
    Log REC1;
    Log REC2;
    vector<uint16_t> logids;

    test_suite_build_log(&REC1, SEVERITY_INFO, 0xCD, 0xAB, 0xEF, P);
    test_suite_build_log(&REC2, SEVERITY_INFO, 0xCD, 0xAB, 0xEF, P);

    /* TEST: build 2 logs and verify their content. */
    Log log;
    test_suite_setup();
    EventManager em0(EVENTSPATH, 0, 0);
    assert(em0.create_log(&REC1) == 1);
    assert(em0.managed_size() == 17);
    assert(em0.managed_count() == 1);
    assert(em0.latest_logid() == 1);
    assert(em0.create_log(&REC2) == 2);
    assert(em0.managed_size() == 34);
    assert(em0.managed_count() == 2);
    assert(em0.latest_logid() == 2);
    assert(em0.load_log(1, &log) == 1);
    assert(log.severity == SEVERITY_INFO);
    assert(log.sensor_type == 0xCD);
    assert(log.sensor_number == 0xAB);
    assert(log.event_dir_type == 0xEF);
    assert(log.event_data[0] == 0x3);
    assert(log.event_data[1] == 0x32);
    assert(log.event_data[2] == 0x34);
    assert(log.logid == 1);
    assert(em0.load_log(2, &log) == 2);

    /* TEST: log count, ID, and size should persist across event manager. */
    EventManager em1(EVENTSPATH, 0, 0);
    assert(em1.managed_size() == 34);
    assert(em1.managed_count() == 2);
    assert(em1.latest_logid() == 2);

    /* TEST: max limits. */
    test_suite_setup();
    EventManager em2(EVENTSPATH, 16, 0);
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
    sd_bus *bus = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *res = NULL;
    uint16_t logid1;
    uint16_t logid2;
    char log_path[64];
    const uint16_t *logids;
    size_t logid_count;

    /* SETUP SD-BUS */
    assert(0 <= sd_bus_open_system(&bus));

    /* TEST: method acceptBMCMessage */
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage",
                &error,
                &res,
                "yyyyyyy",
                SEVERITY_INFO,
                0xCD,
                0xAB,
                0xEF,
                0x12,
                0x34,
                0x56));
    assert(0 <= sd_bus_message_read(res, "q", &logid1));
    sd_bus_message_unref(res);
    assert(logid1 != 0);

    /* TEST: method acceptBMCMessage should accept limited severity */
    assert(sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage",
                &error,
                &res,
                "yyyyyyy",
                99,
                0xCD,
                0xAB,
                0xEF,
                0x12,
                0x34,
                0x56) < 0);
    sd_bus_error_free(&error);

    /* TEST: method delete */
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "delete",
                &error,
                &res,
                "q",
                logid1));
    sd_bus_message_unref(res);
    snprintf(log_path, 64, "/var/lib/obmc/events/%d", logid1);
    assert(access(log_path, F_OK) != 0);

    /* TEST: method clear */
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage",
                &error,
                &res,
                "yyyyyyy",
                SEVERITY_INFO,
                0xCD,
                0xAB,
                0xEF,
                0x12,
                0x34,
                0x56));
    assert(0 <= sd_bus_message_read(res, "q", &logid1));
    sd_bus_message_unref(res);
    assert(logid1 != 0);
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage",
                &error,
                &res,
                "yyyyyyy",
                SEVERITY_INFO,
                0xCD,
                0xAB,
                0xEF,
                0x12,
                0x34,
                0x56));
    assert(0 <= sd_bus_message_read(res, "q", &logid2));
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
                "y",
                0x80));
    sd_bus_message_unref(res);
    snprintf(log_path, 64, "/var/lib/obmc/events/%d", logid2);
    assert(access(log_path, F_OK) != 0);
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
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage",
                &error,
                &res,
                "yyyyyyy",
                SEVERITY_INFO,
                0xCD,
                0xAB,
                0xEF,
                0x12,
                0x34,
                0x56));
    assert(0 <= sd_bus_message_read(res, "q", &logid1));
    sd_bus_message_unref(res);
    assert(logid1 != 0);
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "acceptBMCMessage",
                &error,
                &res,
                "yyyyyyy",
                SEVERITY_INFO,
                0xCD,
                0xAB,
                0xEF,
                0x12,
                0x34,
                0x56));
    assert(0 <= sd_bus_message_read(res, "q", &logid2));
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
