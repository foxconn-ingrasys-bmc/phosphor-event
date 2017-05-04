#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "message.H"

using namespace std;

struct LogHeader {
    uint32_t magic_number;
    uint16_t version;
    uint16_t logid;
    struct timeval timestamp;
    uint16_t message_len;
    uint16_t severity_len;
    uint16_t sensor_type_len;
    uint16_t sensor_number_len;
    uint16_t association_len;
    uint16_t reporter_len;
    uint16_t debug_data_len;
};

const uint32_t MAGIC_NUMBER = 0x4F424D43; // OBMC
const uint16_t VERSION = 1;

static uint16_t getlen (const char *s)
{
    return (uint16_t) (1 + strlen(s));
}

size_t Log::size (void)
{
    return sizeof(LogHeader) +
            getlen(message) +
            getlen(severity) +
            getlen(sensor_type) +
            getlen(sensor_number) +
            getlen(association) +
            getlen(reporter) +
            debug_data_len;
}

uint16_t Log::write (string filepath)
{
    LogHeader hdr = {0};
    ofstream f;
    hdr.magic_number = MAGIC_NUMBER;
    hdr.version = VERSION;
    hdr.logid = logid;
    hdr.timestamp = timestamp;
    hdr.message_len = getlen(message);
    hdr.severity_len = getlen(severity);
    hdr.sensor_type_len = getlen(sensor_type);
    hdr.sensor_number_len = getlen(sensor_number);
    hdr.association_len = getlen(association);
    hdr.reporter_len = getlen(reporter);
    hdr.debug_data_len = debug_data_len;
    f.open(filepath.c_str(), ios::binary);
    if (!f.good()) {
        f.close();
        return 0;
    }
    f.write((char*) &hdr, sizeof(hdr));
    f.write((char*) message, hdr.message_len);
    f.write((char*) severity, hdr.severity_len);
    f.write((char*) sensor_type, hdr.sensor_type_len);
    f.write((char*) sensor_number, hdr.sensor_number_len);
    f.write((char*) association, hdr.association_len);
    f.write((char*) reporter, hdr.reporter_len);
    f.write((char*) debug_data, hdr.debug_data_len);
    f.close();
    return logid;
}

EventManager::EventManager (string path, size_t maxsize, uint16_t maxlogs,
        string sensor_type, string sensor_number,
        void (*on_create_log) (const Log* log),
        void (*on_remove_log) (const Log* log))
{
    uint16_t x;
    Log *log;
    DIR *dir;
    struct dirent *dirent;
    LogIndex index;
    eventpath = path;
    this->maxsize = -1;
    if (maxsize) {
        this->maxsize = maxsize;
    }
    this->maxlogs = -1;
    if (maxlogs) {
        this->maxlogs = maxlogs;
    }
    this->sensor_type = sensor_type;
    this->sensor_number = sensor_number;
    this->on_create_log = on_create_log;
    this->on_remove_log = on_remove_log;
    if ((dir = opendir(eventpath.c_str())) != NULL) {
        while ((dirent = readdir(dir)) != NULL) {
            x = (uint16_t) atoi(dirent->d_name);
            if (is_log(x) && open_log(x, &log) == x) {
                index.logid = x;
                index.timestamp = log->timestamp;
                index.size = log->size();
                logs.push_back(index);
                close_log(log);
            }
        }
        closedir(dir);
    }
}

bool EventManager::is_log (uint16_t logid)
{
    ifstream f;
    LogHeader hdr;
    f.open(log_path(logid).c_str(), ios::binary);
    if (!f.good()) {
        f.close();
        return 0;
    }
    f.read((char*) &hdr, sizeof(hdr));
    f.close();
    return hdr.magic_number == MAGIC_NUMBER;
}

string EventManager::log_path (uint16_t logid)
{
    ostringstream path;
    path << eventpath << "/" << logid;
    return path.str();
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
    vector<LogIndex>::iterator iter;
    size_t size;
    size = 0;
    for (iter = logs.begin() ; iter != logs.end() ; iter++) {
        size += (*iter).size;
    }
    return size;
}

uint16_t EventManager::open_log (uint16_t logid, Log** log)
{
    ifstream f;
    LogHeader hdr;
    f.open(log_path(logid).c_str(), ios::binary);
    if (!f.good()) {
        return 0;
    }
    *log = new Log;
    f.read((char*) &hdr, sizeof(hdr));
    (*log)->logid = hdr.logid;
    (*log)->timestamp = hdr.timestamp;
    (*log)->message = new char[hdr.message_len];
    f.read((*log)->message, hdr.message_len);
    (*log)->severity = new char[hdr.severity_len];
    f.read((*log)->severity, hdr.severity_len);
    (*log)->sensor_type = new char[hdr.sensor_type_len];
    f.read((*log)->sensor_type, hdr.sensor_type_len);
    (*log)->sensor_number = new char[hdr.sensor_number_len];
    f.read((*log)->sensor_number, hdr.sensor_number_len);
    (*log)->association = new char[hdr.association_len];
    f.read((*log)->association, hdr.association_len);
    (*log)->reporter = new char[hdr.reporter_len];
    f.read((*log)->reporter, hdr.reporter_len);
    (*log)->debug_data = new uint8_t[hdr.debug_data_len];
    f.read((char*) (*log)->debug_data, hdr.debug_data_len);
    (*log)->debug_data_len = hdr.debug_data_len;
    f.close();
    return logid;
}

void EventManager::close_log (Log* log)
{
    delete[] log->message;
    delete[] log->severity;
    delete[] log->sensor_type;
    delete[] log->sensor_number;
    delete[] log->association;
    delete[] log->reporter;
    delete[] log->debug_data;
    delete log;
}

uint16_t EventManager::create_log (Log* log)
{
    size_t size;
    uint16_t eldest_logid;
    LogIndex index;
    size = log->size();
    if (maxsize < size) {
        cerr << "DEBUG: log too big to be stored" << endl;
        return 0;
    }
    while (!(managed_size() + size <= maxsize &&
                managed_count() + 1 <= maxlogs)) {
        eldest_logid = this->eldest_logid();
        remove_log(eldest_logid);
    }
    log->logid = next_logid();
    gettimeofday(&log->timestamp, NULL);
    if (log->write(log_path(log->logid)) != log->logid) {
        cerr << "DEBUG: fail to write log file" << endl;
        return 0;
    }
    index.logid = log->logid;
    index.timestamp = log->timestamp;
    index.size = size;
    logs.push_back(index);
    if (on_create_log) {
        on_create_log(log);
    }
    return log->logid;
}

string EventManager::get_sensor_number (void)
{
    return sensor_number;
}

string EventManager::get_sensor_type (void)
{
    return sensor_type;
}

void EventManager::remove_all_logs (void)
{
    LogIndex index;
    Log *log;
    while (!logs.empty()) {
        index = logs.back();
        if (on_remove_log) {
            if (open_log(index.logid, &log) == index.logid) {
                on_remove_log(log);
                close_log(log);
            }
        }
        remove(log_path(index.logid).c_str());
        logs.pop_back();
    }
}

void EventManager::remove_log (uint16_t logid)
{
    Log *log;
    LogIndex index;
    pair<vector<LogIndex>::iterator, vector<LogIndex>::iterator> bounds;
    if (on_remove_log) {
        if (open_log(logid, &log) == logid) {
            on_remove_log(log);
            close_log(log);
        }
    }
    remove(log_path(logid).c_str());
    sort(logs.begin(), logs.end(), LogIndex::compare_by_logid);
    index.logid = logid;
    bounds = equal_range(logs.begin(), logs.end(), index,
            LogIndex::compare_by_logid);
    if ((*bounds.first).logid == logid && bounds.second - bounds.first == 1) {
        logs.erase(bounds.first);
    }
}

uint16_t message_log_clear_all (EventManager* em)
{
    Log log = {
        .message = (char*) "Clear all logs",
        .severity = (char*) "INFO",
        .sensor_type = (char*) em->get_sensor_type().c_str(),
        .sensor_number = (char*) em->get_sensor_number().c_str(),
        .association = (char*) "",
        .reporter = (char*) "BMC",
        .debug_data = NULL,
        .debug_data_len = 0,
    };
    em->remove_all_logs();
    return em->create_log(&log);
}

void message_log_close (EventManager* em, Log* log)
{
    em->close_log(log);
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

uint16_t message_log_open (EventManager* em, uint16_t logid, Log** log)
{
    return em->open_log(logid, log);
}
