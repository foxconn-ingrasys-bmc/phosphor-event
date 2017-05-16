#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <sys/time.h>
#ifdef __cplusplus
#include <fstream>
#include <ios>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#endif

typedef struct Log {
    int16_t logid;
    struct timeval timestamp;
    uint8_t severity;
    // value of severity levels are taken from syslog
    #define SEVERITY_INFO 6
    #define SEVERITY_WARN 4
    #define SEVERITY_CRIT 2
    uint8_t sensor_type;
    uint8_t sensor_number;
    uint8_t event_dir_type;
    uint8_t event_data[3];

#ifdef __cplusplus
    static size_t size (void);

    uint16_t read (std::istream& s);
    uint16_t write (std::ostream& s);
#endif
} Log;

#ifdef __cplusplus
struct LogIndex {
    uint16_t logid;
    struct timeval timestamp;
    size_t size;

    static bool compare_by_logid (LogIndex a, LogIndex b)
    {
        return a.logid < b.logid;
    }

    static bool compare_by_timestamp (LogIndex a, LogIndex b)
    {
        return (a.timestamp.tv_sec < b.timestamp.tv_sec) ||
            (a.timestamp.tv_sec == b.timestamp.tv_sec &&
             a.timestamp.tv_usec < b.timestamp.tv_usec);
    }
};

class EventManager {
private:
    std::string lock_path;
    std::string logs_path;
    int lockfd;
    std::fstream logs_file;
    size_t maxsize;
    uint16_t maxlogs;
    std::vector<LogIndex> logs;

    int open_logs_file (void);
    void close_logs_file (void);
    std::streampos log_position (uint16_t logid);

    void initialize_log_files (void);
    void initialize_log_index (void);
    uint16_t next_logid (void);

public:
    EventManager (std::string path, size_t maxsize, uint16_t maxlogs);

    /* log information retrieval */
    std::vector<uint16_t> logids (void);
    uint16_t eldest_logid (void);
    uint16_t latest_logid (void);
    uint16_t managed_count (void);
    size_t managed_size (void);

    /* low-level log manipulation */
    uint16_t load_log (uint16_t logid, Log* log);
    uint16_t save_log (Log* log);
    void nullify_log (uint16_t logid);

    /* high-level log manipulation */
    uint16_t create_log (Log* log);
    void remove_all_logs (void);
    void remove_log (uint16_t logid);
};
#else
typedef struct EventManager EventManager;
#endif

#ifdef __cplusplus
extern "C" {
#endif
uint16_t message_log_clear_all (EventManager* em, uint8_t sensor_number);
uint16_t message_log_create (EventManager* em, Log* log);
void message_log_delete (EventManager* em, uint16_t logid);
int message_log_get_all_logids (EventManager* em, uint16_t** logids,
        uint16_t* count);
#ifdef __cplusplus
}
#endif

#endif
