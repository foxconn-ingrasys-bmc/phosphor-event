#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <sys/time.h>
#ifdef __cplusplus
#include <string>
#include <vector>
#endif

typedef struct Log {
    char* message;
    char* severity;
    char* sensor_type;
    char* sensor_number;
    char* association;
    char* reporter;
    uint8_t* debug_data;
    size_t debug_data_len;
    struct timeval timestamp;
    int16_t logid;

#ifdef __cplusplus
    size_t size (void);

    uint16_t write (std::string filepath);
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
    std::string eventpath;
    size_t maxsize;
    uint16_t maxlogs;
    std::vector<LogIndex> logs;

    bool is_log (uint16_t logid);
    std::string log_path (uint16_t logid);
    uint16_t next_logid (void);

    void (*on_create_log) (const Log* log);
    void (*on_remove_log) (const Log* log);

public:
    EventManager (std::string path, size_t maxsize, uint16_t maxlogs,
            void (*on_create_log) (const Log* log) = NULL,
            void (*on_remove_log) (const Log* log) = NULL);

    /* log information retrieval */
    std::vector<uint16_t> logids (void);
    uint16_t eldest_logid (void);
    uint16_t latest_logid (void);
    uint16_t managed_count (void);
    size_t managed_size (void);

    /* log manipulation */
    uint16_t open_log (uint16_t logid, Log** log);
    void close_log (Log* log);
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
void message_log_clear_all (EventManager* em);
void message_log_close (EventManager* em, Log* log);
uint16_t message_log_create (EventManager* em, Log* log);
void message_log_delete (EventManager* em, uint16_t logid);
int message_log_get_all_logids (EventManager* em, uint16_t** logids,
        uint16_t* count);
uint16_t message_log_open (EventManager* em, uint16_t logid, Log** log);
#ifdef __cplusplus
}
#endif

#endif
