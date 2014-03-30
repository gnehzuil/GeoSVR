#include "database_manager.h"

extern "C" {
#include <my_global.h>
#include <mysql.h>
}

#include <cstdio>
#include <iostream>

#include "log.h"

using namespace std;

const char* DatabaseManager::db_name_ = "test";
const char* DatabaseManager::table_name_ = "ccymap_rf595672";

DatabaseManager::DatabaseManager()
{
    conn_ = mysql_init(NULL);

    if (conn_ == NULL)
        error("Error %u: %s\n", mysql_errno(conn_), mysql_error(conn_));
}

DatabaseManager::~DatabaseManager()
{
    if (!raw_data_.empty()) {
        for (std::vector<RawData*>::iterator iter = raw_data_.begin();
             iter != raw_data_.end(); ++iter)
            delete *iter;
    }
    if (!deduped_data_.empty()) {
        for (std::vector<DedupedData*>::iterator iter = deduped_data_.begin();
             iter != deduped_data_.end(); ++iter)
            delete *iter;
    }
    mysql_close(conn_);
}

int
DatabaseManager::connect(const char* host, const char* user,
                         const char* passwd)
{
    if (mysql_real_connect(conn_, host, user, passwd,
                           db_name_, 0, NULL, 0) == NULL) {
        error("Error %u: %s\n", mysql_errno(conn_), mysql_error(conn_));
        return -1;
    }

    return 0;
}

int
DatabaseManager::query(const char* sql)
{
    if (mysql_query(conn_, sql)) {
        error("Error %u: %s\n", mysql_errno(conn_), mysql_error(conn_));
        return -1;
    }
    return 0;
}

int
DatabaseManager::get_map_data(void)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char sql[128];

    snprintf(sql, 128, "SELECT * FROM %s", table_name_);
    if (query(sql) < 0)
        return -1;

    result = mysql_store_result(conn_);

    while ((row = mysql_fetch_row(result))) {
        store_raw_data(row);
        dedup_raw_data(row);
    }

    mysql_free_result(result);

    return 0;
}

void
DatabaseManager::store_raw_data(MYSQL_ROW& row)
{
    RawData* raw;

    raw = new RawData();
    if (raw == NULL) {
        error("Out of memory\n");
        return;
    }

    sscanf(row[0], "%d", &raw->mainid);
    sscanf(row[1], "%d", &raw->subid);
    sscanf(row[2], "%lf", &raw->startx);
    sscanf(row[3], "%lf", &raw->starty);
    sscanf(row[4], "%lf", &raw->endx);
    sscanf(row[5], "%lf", &raw->endy);
    sscanf(row[6], "%d", &raw->startid);
    sscanf(row[7], "%d", &raw->endid);
    sscanf(row[9], "%d", &raw->type);
    sscanf(row[13], "%d", &raw->traffic_flow);

    raw_data_.push_back(raw);
}

void
DatabaseManager::dedup_raw_data(MYSQL_ROW& row)
{
    DedupedData* dedup;
    int mainid, subid;

    sscanf(row[0], "%d", &mainid);
    sscanf(row[1], "%d", &subid);

    for (std::vector<DedupedData*>::iterator iter = deduped_data_.begin();
         iter != deduped_data_.end(); ++iter) {
        if ((*iter)->mainid == mainid) {
            if (subid > (*iter)->subid) {
                (*iter)->subid = subid;
                (*iter)->endx = atof(row[4]);
                (*iter)->endy = atof(row[5]);
                (*iter)->endid = atoi(row[7]);
            }

            return;
        }
    }

    dedup = new DedupedData();
    if (dedup == NULL) {
        error("Out of memory\n");
        return;
    }

    sscanf(row[0], "%d", &dedup->mainid);
    sscanf(row[1], "%d", &dedup->subid);
    sscanf(row[2], "%lf", &dedup->startx);
    sscanf(row[3], "%lf", &dedup->starty);
    sscanf(row[4], "%lf", &dedup->endx);
    sscanf(row[5], "%lf", &dedup->endy);
    sscanf(row[6], "%d", &dedup->startid);
    sscanf(row[7], "%d", &dedup->endid);
    sscanf(row[9], "%d", &dedup->type);
    sscanf(row[13], "%d", &dedup->traffic_flow);

    deduped_data_.push_back(dedup);
}
