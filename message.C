#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "message.H"

using namespace std;

static Log BLANK_LOG = {0};

size_t Log::size (void)
{
    // sizeof(logid) +
    // sizeof(timestamp) +
    // sizeof(severity) +
    // sizeof(sensor_type) +
    // sizeof(sensor_number) +
    // sizeof(event_dir_type) +
    // sizeof(event_data)
    return 17;
}

uint16_t Log::read (istream& s)
{
    if (!s.read((char*) &logid, sizeof(logid))) {
        cerr << "DEBUG: failed to read log: logid" << endl;
        return 0;
    }
    if (!s.read((char*) &timestamp, sizeof(timestamp))) {
        cerr << "DEBUG: failed to read log: timestamp" << endl;
        return 0;
    }
    if (!s.read((char*) &severity, sizeof(severity))) {
        cerr << "DEBUG: failed to read log: severity" << endl;
        return 0;
    }
    if (!s.read((char*) &sensor_type, sizeof(sensor_type))) {
        cerr << "DEBUG: failed to read log: sensor type" << endl;
        return 0;
    }
    if (!s.read((char*) &sensor_number, sizeof(sensor_number))) {
        cerr << "DEBUG: failed to read log: sensor number" << endl;
        return 0;
    }
    if (!s.read((char*) &event_dir_type, sizeof(event_dir_type))) {
        cerr << "DEBUG: failed to read log: event_dir_type" << endl;
        return 0;
    }
    if (!s.read((char*) event_data, sizeof(event_data))) {
        cerr << "DEBUG: failed to read log: event data" << endl;
        return 0;
    }
    return logid;
}

uint16_t Log::write (ostream& s)
{
    if (!s.write((char*) &logid, sizeof(logid))) {
        cerr << "DEBUG: failed to write log: logid" << endl;
        return 0;
    }
    if (!s.write((char*) &timestamp, sizeof(timestamp))) {
        cerr << "DEBUG: failed to write log: timestamp" << endl;
        return 0;
    }
    if (!s.write((char*) &severity, sizeof(severity))) {
        cerr << "DEBUG: failed to write log: severity" << endl;
        return 0;
    }
    if (!s.write((char*) &sensor_type, sizeof(sensor_type))) {
        cerr << "DEBUG: failed to write log: sensor type" << endl;
        return 0;
    }
    if (!s.write((char*) &sensor_number, sizeof(sensor_number))) {
        cerr << "DEBUG: failed to write log: sensor number" << endl;
        return 0;
    }
    if (!s.write((char*) &event_dir_type, sizeof(event_dir_type))) {
        cerr << "DEBUG: failed to write log: event_dir_type" << endl;
        return 0;
    }
    if (!s.write((char*) event_data, sizeof(event_data))) {
        cerr << "DEBUG: failed to write log: event data" << endl;
        return 0;
    }
    return logid;
}

EventManager::EventManager (string path, size_t maxsize, uint16_t maxlogs)
{
    lock_path = path + "/lock";
    logs_path = path + "/logs";
    lockfd = -1;
    this->maxsize = -1;
    if (maxsize) {
        this->maxsize = maxsize;
    }
    this->maxlogs = -1;
    if (maxlogs) {
        this->maxlogs = maxlogs;
    }
    initialize_log_files();
    initialize_log_index();
}

int EventManager::open_logs_file (void)
{
    if ((lockfd = open(lock_path.c_str(), 0)) == -1) {
        lockfd = -1;
        cerr << "DEBUG: failed to open logs: " << strerror(errno) << endl;
        return -2;
    }
    if (flock(lockfd, LOCK_EX) != 0) {
        cerr << "DEBUG: failed to open logs: " << strerror(errno) << endl;
        close(lockfd);
        lockfd = -1;
        return -3;
    }
    logs_file.open(logs_path.c_str(), ios::in | ios::out | ios::binary);
    if (!logs_file) {
        cerr << "DEBUG: failed to open logs" << endl;
        logs_file.clear();
        close(lockfd);
        lockfd = -1;
        return -4;
    }
    return 0;
}

void EventManager::close_logs_file (void)
{
    if (logs_file.is_open()) {
        logs_file.close();
    }
    logs_file.clear();
    if (lockfd != -1) {
        close(lockfd);
        lockfd = -1;
    }
}

streampos EventManager::log_position (uint16_t logid)
{
    return Log::size() * (logid - 1);
}

void EventManager::initialize_log_files (void)
{
    ofstream f;
    f.open(lock_path.c_str());
    if (!f) {
        cerr << "DEBUG: failed to create lock file" << endl;
        return;
    }
    f.close();
    f.open(logs_path.c_str(), ios::app);
    if (!f) {
        cerr << "DEBUG: failed to create logs file" << endl;
        return;
    }
    f.close();
}

void EventManager::initialize_log_index (void)
{
    Log log;
    LogIndex index;
    if (open_logs_file() != 0) {
        cerr << "DEBUG: failed to initialize log index" << endl;
        return;
    }
    while (logs_file && !logs_file.eof()) {
        if (log.read(logs_file) == 0) {
            cerr << "DEBUG: initializing log index: skip a log" << endl;
            continue;
        }
        index.logid = log.logid;
        index.timestamp = log.timestamp;
        index.size = Log::size();
        logs.push_back(index);
    }
    close_logs_file();
}

uint16_t EventManager::next_logid (void)
{
    LogIndex index;
    uint16_t i;
    if (logs.size() == maxlogs) {
        return 0;
    }
    if (logs.size() == 0) {
        return 1;
    }
    index.logid = latest_logid();
    sort(logs.begin(), logs.end(), LogIndex::compare_by_logid);
    for (i = 1 ; i <= maxlogs ; i++) {
        index.logid++;
        if (maxlogs < index.logid) {
            index.logid = 1;
        }
        if (!binary_search(logs.begin(), logs.end(), index,
                    LogIndex::compare_by_logid)) {
            return index.logid;
        }
    }
    return 0;
}

vector<uint16_t> EventManager::logids (void)
{
    vector<uint16_t> logids;
    vector<LogIndex>::iterator iter;
    sort(logs.begin(), logs.end(), LogIndex::compare_by_timestamp);
    for (iter = logs.begin() ; iter != logs.end() ; iter++) {
        logids.push_back((*iter).logid);
    }
    return logids;
}

uint16_t EventManager::eldest_logid (void)
{
    if (logs.size() == 0) {
        return 0;
    }
    sort(logs.begin(), logs.end(), LogIndex::compare_by_timestamp);
    return logs.front().logid;
}

uint16_t EventManager::latest_logid (void)
{
    if (logs.size() == 0) {
        return 0;
    }
    sort(logs.begin(), logs.end(), LogIndex::compare_by_timestamp);
    return logs.back().logid;
}

uint16_t EventManager::managed_count (void)
{
    return logs.size();
}

size_t EventManager::managed_size (void)
{
    return Log::size() * managed_count();
}

uint16_t EventManager::load_log (uint16_t logid, Log* log)
{
    if (open_logs_file() != 0) {
        cerr << "DEBUG: failed to load log " << logid << endl;
        return 0;
    }
    logs_file.seekg(log_position(logid));
    if (!logs_file) {
        cerr << "DEBUG: failed to load log " << logid << ": seek" << endl;
        close_logs_file();
        return 0;
    }
    if (log->read(logs_file) != logid) {
        cerr << "DEBUG: failed to load log " << logid << endl;
        close_logs_file();
        return 0;
    }
    close_logs_file();
    return logid;
}

uint16_t EventManager::save_log (Log* log)
{
    if (open_logs_file() != 0) {
        cerr << "DEBUG: failed to save log " << log->logid << endl;
        return 0;
    }
    logs_file.seekp(log_position(log->logid));
    if (!logs_file) {
        cerr << "DEBUG: failed to save log " << log->logid << ": seek" << endl;
        close_logs_file();
        return 0;
    }
    if (log->write(logs_file) != log->logid) {
        cerr << "DEBUG: failed to save log " << log->logid << endl;
        close_logs_file();
        return 0;
    }
    close_logs_file();
    return log->logid;
}

void EventManager::nullify_log (uint16_t logid)
{
    if (open_logs_file() != 0) {
        cerr << "DEBUG: failed to nullify log " << logid << endl;
        return;
    }
    logs_file.seekp(log_position(logid));
    if (!logs_file) {
        cerr << "DEBUG: failed to nullify log " << logid << ": seek" << endl;
        close_logs_file();
        return;
    }
    if (BLANK_LOG.write(logs_file) != BLANK_LOG.logid) {
        cerr << "DEBUG: failed to nullify log " << logid << endl;
        close_logs_file();
        return;
    }
    close_logs_file();
}

uint16_t EventManager::create_log (Log* log)
{
    uint16_t eldest_logid;
    LogIndex index;
    if (maxsize < Log::size()) {
        cerr << "DEBUG: failed to create log: log is too large" << endl;
        return 0;
    }
    while (!(managed_size() + Log::size() <= maxsize &&
                managed_count() + 1 <= maxlogs)) {
        eldest_logid = this->eldest_logid();
        remove_log(eldest_logid);
    }
    log->logid = next_logid();
    gettimeofday(&log->timestamp, NULL);
    if (save_log(log) != log->logid) {
        cerr << "DEBUG: failed to create log" << endl;
        return 0;
    }
    index.logid = log->logid;
    index.timestamp = log->timestamp;
    index.size = Log::size();
    logs.push_back(index);
    return log->logid;
}

void EventManager::remove_all_logs (void)
{
    LogIndex index;
    Log *log;
    while (!logs.empty()) {
        index = logs.back();
        nullify_log(index.logid);
        logs.pop_back();
    }
}

void EventManager::remove_log (uint16_t logid)
{
    LogIndex index;
    pair<vector<LogIndex>::iterator, vector<LogIndex>::iterator> bounds;
    nullify_log(logid);
    sort(logs.begin(), logs.end(), LogIndex::compare_by_logid);
    index.logid = logid;
    bounds = equal_range(logs.begin(), logs.end(), index,
            LogIndex::compare_by_logid);
    if ((*bounds.first).logid == logid && bounds.second - bounds.first == 1) {
        logs.erase(bounds.first);
    }
}

uint16_t message_log_clear_all (EventManager* em, uint8_t sensor_number)
{
    Log log = {
        .logid = 0,
        .timestamp = {0, 0},
        .severity = SEVERITY_INFO,
        .sensor_type = 0x10,
        .sensor_number = sensor_number,
        .event_dir_type = 0x6F,
        .event_data = {0x02, 0x00, 0x00},
    };
    em->remove_all_logs();
    return em->create_log(&log);
}

uint16_t message_log_create (EventManager* em, Log* log)
{
    return em->create_log(log);
}

void message_log_delete (EventManager* em, uint16_t logid)
{
    em->remove_log(logid);
}

int message_log_get_all_logids (EventManager* em, uint16_t** logids,
        uint16_t* count)
{
    vector<uint16_t> logid_vector;
    vector<uint16_t>::iterator iter;
    int i;
    logid_vector = em->logids();
    *logids = (uint16_t*) calloc(sizeof(uint16_t), logid_vector.size());
    if (!*logids) {
        return -1;
    }
    *count = logid_vector.size();
    for (i = 0, iter = logid_vector.begin() ;
            iter != logid_vector.end() ;
            i++, iter++) {
        (*logids)[i] = *iter;
    }
    return 0;
}
