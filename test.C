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

const string LOCK_PATH = "./events.lock";
const string LOG_DIR_PATH = "./events";
const string METADATA_PATH = "./events.metadata";

static void test_suite_build_log (Log* log, uint8_t event_data_1)
{
    strcpy(log->severity, "Severity");
    strcpy(log->entry_type, "Entry Type");
    strcpy(log->entry_code, "Entry Code");
    strcpy(log->sensor_type, "Sensor Type");
    strcpy(log->sensor_number, "0x80");
    strcpy(log->message, "Message");
    sprintf(log->raw_data, "0x01 0x02 0x03 0x%02X 0xFF 0xFF", event_data_1);
}

static void test_suite_setup (void)
{
    char *cmd;
    asprintf(&cmd, "exec rm -r %s 2> /dev/null", LOCK_PATH.c_str());
    system(cmd);
    free(cmd);
    asprintf(&cmd, "exec rm -rf %s 2> /dev/null", LOG_DIR_PATH.c_str());
    system(cmd);
    free(cmd);
    asprintf(&cmd, "exec rm -r %s 2> /dev/null", METADATA_PATH.c_str());
    system(cmd);
    free(cmd);
    asprintf(&cmd, "exec mkdir %s 2> /dev/null", LOG_DIR_PATH.c_str());
    system(cmd);
    free(cmd);
}

static void test_suite (void)
{
    Log LOG1;
    Log LOG2;
    vector<uint16_t> record_ids;

    test_suite_build_log(&LOG1, 0x01);
    test_suite_build_log(&LOG2, 0x02);

    /* TEST: add 2 logs and verify their content. */
    Log log;
    test_suite_setup();
    EventManager em0(LOG_DIR_PATH, LOCK_PATH, METADATA_PATH, 0, 0);
    assert(em0.create_log(&LOG1) == 1);
    assert(em0.managed_size() == 169);
    assert(em0.managed_count() == 1);
    assert(em0.latest_record_id() == 1);
    assert(em0.create_log(&LOG2) == 2);
    assert(em0.managed_size() == 338);
    assert(em0.managed_count() == 2);
    assert(em0.latest_record_id() == 2);
    assert(em0.load_log(1, &log) == 0);
    assert(log.logical_timestamp == 1);
    assert(strcmp(log.id, "1") == 0);
    assert(strcmp(log.name, "Log Entry 1") == 0);
    assert(strcmp(log.record_id, "1") == 0);
    assert(strcmp(log.severity, "Severity") == 0);
    assert(strcmp(log.entry_type, "Entry Type") == 0);
    assert(strcmp(log.entry_code, "Entry Code") == 0);
    assert(strcmp(log.sensor_type, "Sensor Type") == 0);
    assert(strcmp(log.sensor_number, "0x80") == 0);
    assert(strcmp(log.message, "Message") == 0);
    assert(strlen(log.raw_data) == 79);

    /* TEST: log count, record ID, and size should persist. */
    EventManager em1(LOG_DIR_PATH, LOCK_PATH, METADATA_PATH, 0, 0);
    assert(em1.managed_size() == 338);
    assert(em1.managed_count() == 2);
    assert(em1.latest_record_id() == 2);

    /* TEST: max size. */
    test_suite_setup();
    EventManager em2(LOG_DIR_PATH, LOCK_PATH, METADATA_PATH, 0, 10);
    assert(em2.create_log(&LOG1) == 0);

    /* TEST: next record ID is derived from the latest log. */
    test_suite_setup();
    EventManager em3(LOG_DIR_PATH, LOCK_PATH, METADATA_PATH, 4, 0);
    assert(em3.create_log(&LOG1) == 1);
    assert(em3.create_log(&LOG1) == 2);
    assert(em3.create_log(&LOG1) == 3);
    assert(em3.create_log(&LOG1) == 4);
    assert(em3.create_log(&LOG1) == 1);

    /* TEST: circular event log, rollover count, and timestamp. */
    test_suite_setup();
    EventManager em4(LOG_DIR_PATH, LOCK_PATH, METADATA_PATH, 4, 0);
    assert(em4.rollover_count() == 0);
    assert(em4.create_log(&LOG1) == 1);
    assert(LOG1.logical_timestamp == 1);
    assert(em4.rollover_count() == 0);
    assert(em4.create_log(&LOG1) == 2);
    assert(LOG1.logical_timestamp == 2);
    assert(em4.rollover_count() == 0);
    assert(em4.create_log(&LOG1) == 3);
    assert(LOG1.logical_timestamp == 3);
    assert(em4.rollover_count() == 0);
    assert(em4.create_log(&LOG1) == 4);
    assert(LOG1.logical_timestamp == 4);
    assert(em4.rollover_count() == 0);
    assert(em4.create_log(&LOG1) == 1);
    assert(LOG1.logical_timestamp == 5);
    assert(em4.rollover_count() == 1);
    assert(em4.create_log(&LOG1) == 2);
    assert(LOG1.logical_timestamp == 6);
    assert(em4.rollover_count() == 1);
    assert(em4.create_log(&LOG1) == 3);
    assert(LOG1.logical_timestamp == 7);
    assert(em4.rollover_count() == 1);
    assert(em4.create_log(&LOG1) == 4);
    assert(LOG1.logical_timestamp == 8);
    assert(em4.rollover_count() == 1);
    assert(em4.create_log(&LOG1) == 1);
    assert(LOG1.logical_timestamp == 9);
    assert(em4.rollover_count() == 2);
    assert(em4.create_log(&LOG1) == 2);
    assert(LOG1.logical_timestamp == 10);
    assert(em4.rollover_count() == 2);
    assert(em4.create_log(&LOG1) == 3);
    assert(LOG1.logical_timestamp == 11);
    assert(em4.rollover_count() == 2);
    assert(em4.create_log(&LOG1) == 4);
    assert(LOG1.logical_timestamp == 12);
    assert(em4.rollover_count() == 2);

    /* TEST: rollover count should persist. */
    EventManager em5(LOG_DIR_PATH, LOCK_PATH, METADATA_PATH, 0, 0);
    assert(em5.rollover_count() == 2);

    /* TEST: remove all and rollover count. */
    test_suite_setup();
    EventManager em7(LOG_DIR_PATH, LOCK_PATH, METADATA_PATH, 4, 0);
    assert(em7.create_log(&LOG1) == 1);
    assert(em7.create_log(&LOG1) == 2);
    assert(em7.create_log(&LOG1) == 3);
    assert(em7.create_log(&LOG1) == 4);
    assert(em7.create_log(&LOG1) == 1);
    em7.remove_all_logs();
    assert(em7.managed_count() == 0);
    assert(em7.rollover_count() == 0);
}

static void test_suite_systemd (void)
{
    sd_bus *bus = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *res = NULL;
    uint16_t record_id_1;
    uint16_t record_id_2;
    const uint16_t *record_ids;
    const uint64_t *timestamps;
    size_t log_count;
    uint8_t rollover_count;

    /* SETUP SD-BUS */
    assert(0 <= sd_bus_open_system(&bus));

    /* TEST: method create */
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "create",
                &error,
                &res,
                "sssssss",
                "Severity",
                "Entry Type",
                "Entry Code",
                "Sensor Type",
                "0x80",
                "Message",
                "0x01 0x02 0x03 0x04 0x05 0x06"));
    assert(0 <= sd_bus_message_read(res, "q", &record_id_1));
    sd_bus_message_unref(res);
    assert(record_id_1 != 0);

    /* TEST: method clear */
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "create",
                &error,
                &res,
                "sssssss",
                "Severity",
                "Entry Type",
                "Entry Code",
                "Sensor Type",
                "0x80",
                "Message",
                "0x01 0x02 0x03 0x04 0x05 0x06"));
    assert(0 <= sd_bus_message_read(res, "q", &record_id_1));
    sd_bus_message_unref(res);
    assert(record_id_1 != 0);
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "create",
                &error,
                &res,
                "sssssss",
                "Severity",
                "Entry Type",
                "Entry Code",
                "Sensor Type",
                "0x80",
                "Message",
                "0x01 0x02 0x03 0x04 0x05 0x06"));
    assert(0 <= sd_bus_message_read(res, "q", &record_id_2));
    sd_bus_message_unref(res);
    assert(record_id_2 != 0);
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

    /* TEST: method get_record_ids_and_logical_timestamps */
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "create",
                &error,
                &res,
                "sssssss",
                "Severity",
                "Entry Type",
                "Entry Code",
                "Sensor Type",
                "0x80",
                "Message",
                "0x01 0x02 0x03 0x04 0x05 0x06"));
    assert(0 <= sd_bus_message_read(res, "q", &record_id_1));
    sd_bus_message_unref(res);
    assert(record_id_1 != 0);
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "create",
                &error,
                &res,
                "sssssss",
                "Severity",
                "Entry Type",
                "Entry Code",
                "Sensor Type",
                "0x80",
                "Message",
                "0x01 0x02 0x03 0x04 0x05 0x06"));
    assert(0 <= sd_bus_message_read(res, "q", &record_id_2));
    sd_bus_message_unref(res);
    assert(record_id_2 != 0);
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "get_record_ids_and_logical_timestamps",
                &error,
                &res,
                NULL));
    assert(0 <= sd_bus_message_read_array(res, SD_BUS_TYPE_UINT16,
                (const void**) &record_ids, &log_count));
    assert(0 <= sd_bus_message_read_array(res, SD_BUS_TYPE_UINT64,
                (const void**) &timestamps, &log_count));
    log_count /= sizeof(uint64_t);
    assert(log_count == 3); // the first log is "clearing all logs"
    assert(record_ids[1] == record_id_1);
    assert(record_ids[2] == record_id_2);
    sd_bus_message_unref(res);

    /* TEST: method get_rollover_count */
    rollover_count = 0xFF;
    assert(0 <= sd_bus_call_method(
                bus,
                "org.openbmc.records.events",
                "/org/openbmc/records/events",
                "org.openbmc.recordlog",
                "get_rollover_count",
                &error,
                &res,
                NULL));
    assert(0 <= sd_bus_message_read(res, "y", &rollover_count));
    assert(rollover_count == 0);
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
