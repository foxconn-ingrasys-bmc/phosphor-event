#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <sys/time.h>
#ifdef __cplusplus
#include <fstream>
#include <ios>
#include <istream>
#include <list>
#include <ostream>
#include <string>
#endif

typedef struct Log {
    uint64_t logical_timestamp;
    #define LOG_ID_MAX_LENGTH 20
    char id[LOG_ID_MAX_LENGTH];
    #define LOG_NAME_MAX_LENGTH 32
    char name[LOG_NAME_MAX_LENGTH];
    #define LOG_RECORD_ID_MAX_LENGTH 20
    char record_id[LOG_RECORD_ID_MAX_LENGTH];
    #define LOG_SEVERITY_MAX_LENGTH 16
    char severity[LOG_SEVERITY_MAX_LENGTH];
    #define LOG_SEVERITY_CRIT "Critical"
    #define LOG_SEVERITY_INFO "Info"
    #define LOG_SEVERITY_OKAY "OK"
    #define LOG_SEVERITY_WARN "Warning"
    #define LOG_CREATED_MAX_LENGTH 32
    char created[LOG_CREATED_MAX_LENGTH];
    #define LOG_ENTRY_TYPE_MAX_LENGTH 32
    char entry_type[LOG_ENTRY_TYPE_MAX_LENGTH];
    #define LOG_ENTRY_CODE_MAX_LENGTH 128
    char entry_code[LOG_ENTRY_CODE_MAX_LENGTH];
    #define LOG_SENSOR_TYPE_MAX_LENGTH 32
    char sensor_type[LOG_SENSOR_TYPE_MAX_LENGTH];
    #define LOG_SENSOR_NUMBER_MAX_LENGTH 8
    char sensor_number[LOG_SENSOR_NUMBER_MAX_LENGTH];
    #define LOG_MESSAGE_MAX_LENGTH 256
    char message[LOG_MESSAGE_MAX_LENGTH];
    #define LOG_RAW_DATA_MAX_LENGTH 128
    char raw_data[LOG_RAW_DATA_MAX_LENGTH];

#ifdef __cplusplus
    size_t size (void);

    int read (std::istream& s);
    int write (std::ostream& s);
#endif
} Log;

#ifdef __cplusplus
class LogIndex {
public:
    uint64_t logical_timestamp;
    uint16_t record_id;
    size_t size;

    static bool compare_by_logical_timestamp (LogIndex a, LogIndex b)
    {
        return a.logical_timestamp < b.logical_timestamp;
    }
};

class EventManager {
private:
    std::string log_dir_path;
    std::string lock_path;
    std::string metadata_path;
    int lock_fd;
    uint64_t logical_clock;
    uint16_t max_logs;
    size_t max_size;
    std::list<LogIndex> logs;
    uint8_t rollover;

    int acquire_lock (void);
    uint16_t autofill_log (Log* log);
    int check_sanity (void);
    int initialize_index (void);
    int initialize_lock (void);
    int initialize_metadata (void);
    std::string log_path (uint64_t logical_timestamp);
    uint64_t next_logical_timestamp (void);
    uint16_t next_record_id (void);
    int release_lock (void);
    int remove_log (uint64_t logical_timestamp);
    int set_rollover_count (uint8_t rollover);

public:
    EventManager (
            std::string log_dir_path,
            std::string lock_path,
            std::string metadata_path,
            uint16_t max_logs,
            size_t max_size);

    /* log information retrieval */
    uint16_t latest_record_id (void);
    uint16_t managed_count (void);
    size_t managed_size (void);
    std::list<uint16_t> record_ids (void);
    uint8_t rollover_count (void);
    std::list<uint64_t> timestamps (void);

    /* low-level log manipulation */
    int load_log (uint64_t logical_timestamp, Log* log);
    int save_log (Log* log);

    /* high-level log manipulation */
    uint16_t create_log (Log* log);
    int remove_all_logs (void);
};
#else
typedef struct EventManager EventManager;
#endif

#ifdef __cplusplus
extern "C" {
#endif
uint16_t message_clear_all (EventManager* em, uint8_t sensor_number);
uint16_t message_create (EventManager* em, Log* log);
int message_get_record_ids_and_logical_timestamps (
        EventManager* em,
        uint16_t** record_ids,
        uint64_t** timestamps,
        uint16_t* count);
uint8_t message_get_rollover_count (EventManager* em);
#ifdef __cplusplus
}
#endif

#endif
