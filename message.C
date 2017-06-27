#include <algorithm>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include "message.H"

using namespace std;

size_t Log::size (void)
{
    // size of content written to file
    return strlen(id) + 1 +
        strlen(name) + 1 +
        strlen(record_id) + 1 +
        strlen(severity) + 1 +
        strlen(created) + 1 +
        strlen(entry_type) + 1 +
        strlen(entry_code) + 1 +
        strlen(sensor_type) + 1 +
        strlen(sensor_number) + 1 +
        strlen(message) + 1 +
        strlen(raw_data) + 1;
}

int Log::read (istream& s)
{
    if (!s.getline(id, LOG_ID_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: id" << endl;
        return -1;
    }
    if (!s.getline(name, LOG_NAME_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: name" << endl;
        return -2;
    }
    if (!s.getline(record_id, LOG_RECORD_ID_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: record_id" << endl;
        return -3;
    }
    if (!s.getline(severity, LOG_SEVERITY_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: severity" << endl;
        return -4;
    }
    if (!s.getline(created, LOG_CREATED_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: created" << endl;
        return -5;
    }
    if (!s.getline(entry_type, LOG_ENTRY_TYPE_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: entry_type" << endl;
        return -6;
    }
    if (!s.getline(entry_code, LOG_ENTRY_CODE_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: entry_code" << endl;
        return -7;
    }
    if (!s.getline(sensor_type, LOG_SENSOR_TYPE_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: sensor_type" << endl;
        return -8;
    }
    if (!s.getline(sensor_number, LOG_SENSOR_NUMBER_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: sensor_number" << endl;
        return -9;
    }
    if (!s.getline(message, LOG_MESSAGE_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: message" << endl;
        return -10;
    }
    if (!s.getline(raw_data, LOG_RAW_DATA_MAX_LENGTH)) {
        cerr << "DEBUG: failed to read log: raw_data" << endl;
        return -11;
    }
    return 0;
}

int Log::write (ostream& s)
{
    if (!(s << id << endl)) {
        cerr << "DEBUG: failed to write log: id" << endl;
        return -1;
    }
    if (!(s << name << endl)) {
        cerr << "DEBUG: failed to write log: name" << endl;
        return -2;
    }
    if (!(s << record_id << endl)) {
        cerr << "DEBUG: failed to write log: record_id" << endl;
        return -3;
    }
    if (!(s << severity << endl)) {
        cerr << "DEBUG: failed to write log: severity" << endl;
        return -4;
    }
    if (!(s << created << endl)) {
        cerr << "DEBUG: failed to write log: created" << endl;
        return -5;
    }
    if (!(s << entry_type << endl)) {
        cerr << "DEBUG: failed to write log: entry_type" << endl;
        return -6;
    }
    if (!(s << entry_code << endl)) {
        cerr << "DEBUG: failed to write log: entry_code" << endl;
        return -7;
    }
    if (!(s << sensor_type << endl)) {
        cerr << "DEBUG: failed to write log: sensor_type" << endl;
        return -8;
    }
    if (!(s << sensor_number << endl)) {
        cerr << "DEBUG: failed to write log: sensor_number" << endl;
        return -9;
    }
    if (!(s << message << endl)) {
        cerr << "DEBUG: failed to write log: message" << endl;
        return -10;
    }
    if (!(s << raw_data << endl)) {
        cerr << "DEBUG: failed to write log: raw_data" << endl;
        return -11;
    }
    return 0;
}

EventManager::EventManager (
        string log_dir_path,
        string lock_path,
        string metadata_path,
        uint16_t max_logs,
        size_t max_size)
{
    this->log_dir_path = log_dir_path;
    this->lock_path = lock_path;
    this->metadata_path = metadata_path;
    this->lock_fd = -1;
    this->max_logs = max_logs ? max_logs : -1;
    this->max_size = max_size ? max_size : -1;
    this->rollover = 0;
    initialize_lock();
    initialize_index();
    initialize_metadata();
    check_sanity();
}

int EventManager::acquire_lock (void)
{
    /* This lock is used to synchronize access to log files among this daemon
     * and other processes such as redfish and event CLI tool.
     *
     * Other processes must acquire this lock before accessing any log file.
     * This daemon also acquires this lock when adding or deleting logs.
     */
    if ((lock_fd = open(lock_path.c_str(), O_RDONLY)) == -1) {
        cerr << "DEBUG: failed to acquire lock, can't open" << endl;
        return -1;
    }
    if (flock(lock_fd, LOCK_EX) != 0) {
        cerr << "DEBUG: failed to acquire lock, can't flock" << endl;
        close(lock_fd);
        lock_fd = -1;
        return -2;
    }
    return 0;
}

uint16_t EventManager::autofill_log (Log* log)
{
    uint16_t record_id;
    struct timespec now;
    struct tm *tm;
    char raw_data[LOG_RAW_DATA_MAX_LENGTH];
    log->logical_timestamp = next_logical_timestamp();
    record_id = next_record_id();
    if (LOG_ID_MAX_LENGTH <= snprintf(log->id, LOG_ID_MAX_LENGTH,
                "%u", record_id)) {
        cerr << "DEBUG: failed to autofill log: id" << endl;
        return 0;
    }
    if (LOG_NAME_MAX_LENGTH <= snprintf(log->name, LOG_NAME_MAX_LENGTH,
                "Log Entry %u", record_id)) {
        cerr << "DEBUG: failed to autofill log: name" << endl;
        return 0;
    }
    if (LOG_RECORD_ID_MAX_LENGTH <= snprintf(log->record_id,
                LOG_RECORD_ID_MAX_LENGTH, "%u", record_id)) {
        cerr << "DEBUG: failed to autofill log: record_id" << endl;
        return 0;
    }
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        cerr << "DEBUG: failed to autofill log: created" << endl;
        return 0;
    }
    if (!(tm = gmtime(&now.tv_sec))) {
        cerr << "DEBUG: failed to autofill log: created" << endl;
        return 0;
    }
    if (LOG_CREATED_MAX_LENGTH <= snprintf(log->created,
                LOG_CREATED_MAX_LENGTH, "%04d-%02d-%02dT%02d:%02d",
                tm->tm_year + 1900,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min)) {
        cerr << "DEBUG: failed to autofill log: created" << endl;
        return 0;
    }
    if (strlen(log->raw_data) != 5 * 16 - 1) {
        if (LOG_RAW_DATA_MAX_LENGTH <= snprintf(raw_data,
                    LOG_RAW_DATA_MAX_LENGTH,
                    "0x%02X 0x%02X 0x02 0x%02X 0x%02X 0x%02X 0x%02X"
                    " 0x41 0x00 0x04 %s",
                    (record_id & 0xFF00) >> 8,
                    (record_id & 0x00FF),
                    (now.tv_sec & 0xFF000000) >> 24,
                    (now.tv_sec & 0x00FF0000) >> 16,
                    (now.tv_sec & 0x0000FF00) >> 8,
                    (now.tv_sec & 0x000000FF),
                    log->raw_data)) {
            cerr << "DEBUG: failed to autofill log: raw_data" << endl;
            return 0;
        }
        if (LOG_RAW_DATA_MAX_LENGTH <= snprintf(log->raw_data,
                    LOG_RAW_DATA_MAX_LENGTH,
                    "%s",
                    raw_data)) {
            cerr << "DEBUG: failed to autofill log: raw_data" << endl;
            return 0;
        }
    }
    return record_id;
}

int EventManager::check_sanity (void)
{
    if (!(managed_count() <= max_logs)) {
        cerr << "ASSERT: sanity_check: number of logs (" << managed_count() <<
            ") exceeds " << max_logs << endl;
        return -1;
    }
    if (!(managed_size() <= max_size)) {
        cerr << "ASSERT: sanity_check: total log size (" << managed_size() <<
            " bytes) exceeds " << max_size << " bytes" << endl;
        return -2;
    }
    return 0;
}

int EventManager::initialize_index (void)
{
    DIR* dir;
    struct dirent *dirent;
    LogIndex index;
    Log log;
    vector<LogIndex> logs;
    vector<LogIndex>::iterator iter;
    if (!(dir = opendir(log_dir_path.c_str()))) {
        cerr << "DEBUG: failed to initialize index" << endl;
        return -2;
    }
    while ((dirent = readdir(dir))) {
        index.logical_timestamp = atoll(dirent->d_name);
        if (index.logical_timestamp == 0) {
            // 0 is an invalid timestamp reserved for d_name such as ".."
            continue;
        }
        if (load_log(index.logical_timestamp, &log) == 0) {
            index.record_id = atol(log.record_id);
            index.size = log.size();
            logs.push_back(index);
        }
        else {
            cerr << "ASSERT: initialize_index: failed to load " <<
                log_dir_path << "/" << dirent->d_name << endl;
            continue;
        }
    }
    closedir(dir);
    sort(logs.begin(), logs.end(), LogIndex::compare_by_logical_timestamp);
    for (iter = logs.begin() ; iter != logs.end() ; iter++) {
        this->logs.push_back((*iter));
    }
    return 0;
}

int EventManager::initialize_lock (void)
{
    ofstream f;
    f.open(lock_path.c_str());
    if (!f) {
        cerr << "DEBUG: failed to initialize lock" << endl;
        return -1;
    }
    f.close();
    return 0;
}

int EventManager::initialize_metadata (void)
{
    fstream f;
    f.open(metadata_path.c_str(),
            fstream::out | fstream::binary | fstream::app);
    if (!f) {
        cerr << "DEBUG: failed to initialize metadata: create" << endl;
        return -1;
    }
    f.close();
    f.open(metadata_path.c_str(),
            fstream::in | fstream::out | fstream::binary | fstream::ate);
    if (!f) {
        cerr << "DEBUG: failed to initialize metadata: open" << endl;
        return -2;
    }
    if (f.tellp() != 0) {
        if (!f.seekg(0)) {
            cerr << "DEBUG: failed to initialize metadata: seekg" << endl;
            f.close();
            return -3;
        }
        if (!f.read((char*) &rollover, sizeof(rollover))) {
            cerr << "DEBUG: failed to initialize metadata: read" << endl;
            f.close();
            return -4;
        }
    }
    else {
        if (!f.write((char*) &rollover, sizeof(rollover))) {
            cerr << "DEBUG: failed to initialize metadata: write" << endl;
            f.close();
            return -5;
        }
    }
    f.close();
    return 0;
}

string EventManager::log_path (uint64_t logical_timestamp)
{
    stringstream path;
    path << log_dir_path << '/' << logical_timestamp;
    return path.str();
}

uint64_t EventManager::next_logical_timestamp (void)
{
    uint64_t timestamp;
    if (logs.size() != 0) {
        timestamp = logs.back().logical_timestamp + 1;
        if (timestamp == 0) {
            timestamp = 1;
            cerr << "ASSERT: next_logical_timestamp: logical clock "
                "wraps" << endl;
        }
        return timestamp;
    }
    else {
        return 1;
    }
}

uint16_t EventManager::next_record_id (void)
{
    uint16_t record_id;
    if (logs.size() != 0) {
        record_id = logs.back().record_id + 1;
        if (max_logs < record_id) {
            record_id = 1;
        }
        return record_id;
    }
    else {
        return 1;
    }
}

int EventManager::release_lock (void)
{
    if (lock_fd != -1) {
        close(lock_fd);
        lock_fd = -1;
    }
    else {
        cerr << "ASSERT: release_lock: lock is already released" << endl;
    }
    return 0;
}

int EventManager::remove_log (uint64_t logical_timestamp)
{
    string path;
    path = log_path(logical_timestamp);
    if (unlink(path.c_str()) != 0) {
        if (errno != ENOENT) {
            cerr << "DEBUG: failed to remove log " << path << " due to " <<
                strerror(errno) << endl;
            return -1;
        }
        else {
            cerr << "ASSERT: remove_log: log " << path <<
                " doesn't exist in the first place" << endl;
        }
    }
    return 0;
}

int EventManager::set_rollover_count (uint8_t rollover)
{
    ofstream f;
    this->rollover = rollover;
    f.open(metadata_path.c_str(), ofstream::binary | ofstream::trunc);
    if (!f) {
        cerr << "DEBUG: failed to increase rollover count, open" << endl;
        return -1;
    }
    if (!f.write((char*) &rollover, sizeof(rollover))) {
        cerr << "DEBUG: failed to increase rollover count, write" << endl;
        return -2;
    }
    f.close();
    return 0;
}

uint16_t EventManager::latest_record_id (void)
{
    return logs.back().record_id;
}

uint16_t EventManager::managed_count (void)
{
    return logs.size();
}

size_t EventManager::managed_size (void)
{
    list<LogIndex>::iterator iter;
    size_t size;
    size = 0;
    for (iter = logs.begin() ; iter != logs.end() ; iter++) {
        size += (*iter).size;
    }
    return size;
}

int EventManager::load_log (uint64_t logical_timestamp, Log* log)
{
    string path;
    ifstream f;
    path = log_path(logical_timestamp);
    f.open(path.c_str());
    if (!f) {
        cerr << "DEBUG: failed to load log " << path << ", can't open" << endl;
        return -1;
    }
    if (log->read(f) != 0) {
        cerr << "DEBUG: failed to load log " << path << ", can't read" << endl;
        f.close();
        return -2;
    }
    f.close();
    log->logical_timestamp = logical_timestamp;
    return 0;
}

int EventManager::save_log (Log* log)
{
    string path;
    ofstream f;
    path = log_path(log->logical_timestamp);
    f.open(path.c_str());
    if (!f) {
        cerr << "DEBUG: failed to save log " << log->record_id << " to " <<
            path << ", can't open" << endl;
        return -1;
    }
    if (log->write(f) != 0) {
        cerr << "DEBUG: failed to save log " << log->record_id << " to " <<
            path << ", can't write" << endl;
        f.close();
        return -2;
    }
    f.close();
    return 0;
}

list<uint16_t> EventManager::record_ids (void)
{
    list<uint16_t> record_ids;
    list<LogIndex>::iterator iter;
    for (iter = logs.begin() ; iter != logs.end() ; iter++) {
        record_ids.push_back((*iter).record_id);
    }
    return record_ids;
}

uint8_t EventManager::rollover_count (void)
{
    return rollover;
}

list<uint64_t> EventManager::timestamps (void)
{
    list<uint64_t> timestamps;
    list<LogIndex>::iterator iter;
    for (iter = logs.begin() ; iter != logs.end() ; iter++) {
        timestamps.push_back((*iter).logical_timestamp);
    }
    return timestamps;
}

uint16_t EventManager::create_log (Log* log)
{
    uint16_t record_id;
    bool should_rollover;
    uint64_t eldest_timestamp;
    uint16_t eldest_record_id;
    LogIndex index;
    if ((record_id = autofill_log(log)) == 0) {
        cerr << "DEBUG: failed to create log: autofill failed" << endl;
        return 0;
    }
    if (max_size < log->size()) {
        cerr << "DEBUG: failed to create log: log is too large" << endl;
        return 0;
    }
    should_rollover = record_id == 1 && logs.back().record_id == max_logs;
    if (acquire_lock() != 0) {
        cerr << "DEBUG: failed to create log: can't acquire_lock" << endl;
        return 0;
    }
    while (!(managed_size() + log->size() <= max_size &&
                managed_count() + 1 <= max_logs)) {
        eldest_timestamp = logs.front().logical_timestamp;
        eldest_record_id = logs.front().record_id;
        if (remove_log(eldest_timestamp) == 0) {
            logs.pop_front();
            cerr << "DEBUG: create_log: removed log " <<
                eldest_record_id << endl;
        }
        else {
            cerr << "DEBUG: failed to create log: failed to remove "
                "log " << eldest_record_id << endl;
            release_lock();
            return 0;
        }
    }
    if (save_log(log) != 0) {
        cerr << "DEBUG: failed to create log: can't save_log" << endl;
        release_lock();
        return 0;
    }
    release_lock();
    index.logical_timestamp = log->logical_timestamp;
    index.record_id = record_id;
    index.size = log->size();
    logs.push_back(index);
    if (should_rollover) {
        if (set_rollover_count(rollover + 1) != 0) {
            cerr << "ASSERT: create_log: failed to save rollover count" <<
                endl;
        }
    }
    cerr << "DEBUG: create_log: created log " << record_id << " at " <<
        log_path(log->logical_timestamp) << ", raw data " <<
        log->raw_data << endl;
    return record_id;
}

int EventManager::remove_all_logs (void)
{
    LogIndex index;
    Log *log;
    int err;
    if (acquire_lock() != 0) {
        cerr << "DEBUG: failed to remove all logs: can't acquire_lock" << endl;
        return -1;
    }
    err = 0;
    while (!logs.empty()) {
        index = logs.back();
        logs.pop_back();
        if (remove_log(index.logical_timestamp) == 0) {
            cerr << "DEBUG: remove_all_logs: removed log " <<
                index.record_id << " at " <<
                log_path(index.logical_timestamp) << endl;
        }
        else {
            cerr << "ASSERT: remove_all_logs: failed to remove log " <<
                index.record_id << " at " <<
                log_path(index.logical_timestamp) << endl;
            err = -2;
        }
    }
    release_lock();
    if (set_rollover_count(0) != 0) {
        cerr << "ASSERT: remove_all_logs: failed to save rollover count" <<
            endl;
    }
    if (err) {
        cerr << "DEBUG: failed to remove all logs" << endl;
    }
    return err;
}

uint16_t message_clear_all (EventManager* em, uint8_t sensor_number)
{
    Log log;
    if (LOG_SEVERITY_MAX_LENGTH <= snprintf(log.severity,
                LOG_SEVERITY_MAX_LENGTH, LOG_SEVERITY_INFO)) {
        cerr << "DEBUG: failed to clear all logs: severity" << endl;
        return 0;
    }
    if (LOG_ENTRY_TYPE_MAX_LENGTH <= snprintf(log.entry_type,
                LOG_ENTRY_TYPE_MAX_LENGTH, "SEL")) {
        cerr << "DEBUG: failed to clear all logs: entry_type" << endl;
        return 0;
    }
    if (LOG_ENTRY_CODE_MAX_LENGTH <= snprintf(log.entry_code,
                LOG_ENTRY_CODE_MAX_LENGTH, "Log Area Reset / Cleared")) {
        cerr << "DEBUG: failed to clear all logs: entry_code" << endl;
        return 0;
    }
    if (LOG_SENSOR_TYPE_MAX_LENGTH <= snprintf(log.sensor_type,
                LOG_SENSOR_TYPE_MAX_LENGTH, "Event Log")) {
        cerr << "DEBUG: failed to clear all logs: sensor type" << endl;
        return 0;
    }
    if (LOG_SENSOR_NUMBER_MAX_LENGTH <= snprintf(log.sensor_number,
                LOG_SENSOR_NUMBER_MAX_LENGTH, "0x%02X", sensor_number)) {
        cerr << "DEBUG: failed to clear all logs: sensor_number" << endl;
        return 0;
    }
    if (LOG_MESSAGE_MAX_LENGTH <= snprintf(log.message, LOG_MESSAGE_MAX_LENGTH,
                "Event Log asserted Log Area Reset / Cleared")) {
        cerr << "DEBUG: failed to clear all logs: message" << endl;
        return 0;
    }
    if (LOG_RAW_DATA_MAX_LENGTH <= snprintf(log.raw_data,
                LOG_RAW_DATA_MAX_LENGTH, "0x10 0x%02X 0x6F 0x02 0xFF 0xFF",
                sensor_number)) {
        cerr << "DEBUG: failed to clear all logs: raw_data" << endl;
        return 0;
    }
    if (em->remove_all_logs() != 0) {
        return 0;
    }
    return em->create_log(&log);
}

uint16_t message_create (EventManager* em, Log* log)
{
    return em->create_log(log);
}

int message_get_record_ids_and_logical_timestamps (
        EventManager* em,
        uint16_t** record_ids,
        uint64_t** timestamps,
        uint16_t* count)
{
    list<uint16_t> recrod_id_list;
    list<uint64_t> timestamp_list;
    int i;
    list<uint16_t>::iterator recrod_id_iter;
    list<uint64_t>::iterator timestamp_iter;
    recrod_id_list = em->record_ids();
    timestamp_list = em->timestamps();
    if (recrod_id_list.size() == timestamp_list.size()) {
        *count = recrod_id_list.size();
    }
    else {
        cerr << "ASSERT: message_get_record_ids_and_logical_timestamps: "
            "mismatch between numbers of record IDs and timestamps" << endl;
        return -1;
    }
    *record_ids = (uint16_t*) calloc(sizeof(uint16_t), *count);
    if (!*record_ids) {
        cerr << "DEBUG: failed to get record IDs and timestamps: record_ids" <<
            endl;
        return -2;
    }
    *timestamps = (uint64_t*) calloc(sizeof(uint64_t), *count);
    if (!*timestamps) {
        cerr << "DEBUG: failed to get record IDs and timestamps: timestamps" <<
            endl;
        return -3;
    }
    for (i = 0,
            recrod_id_iter = recrod_id_list.begin(),
            timestamp_iter = timestamp_list.begin() ;
            recrod_id_iter != recrod_id_list.end() &&
            timestamp_iter != timestamp_list.end() ;
            i++, recrod_id_iter++, timestamp_iter++) {
        (*record_ids)[i] = *recrod_id_iter;
        (*timestamps)[i] = *timestamp_iter;
    }
    return 0;
}

uint8_t message_get_rollover_count (EventManager* em)
{
    return em->rollover_count();
}
