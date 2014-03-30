#ifndef GEOSVR_DATABASE_MANAGER_H_
#define GEOSVR_DATABASE_MANAGER_H_

#include <algorithm>
#include <iostream>
#include <vector>

typedef struct st_mysql MYSQL;
typedef char **MYSQL_ROW;

struct RawData {
    int mainid;
    int subid;
    double startx;
    double starty;
    double endx;
    double endy;
    int startid;
    int endid;
    int type; // NOTE: map to 'rclass' in database
    int traffic_flow;
};

struct DedupedData {
    int mainid;
    int subid;
    double startx;
    double starty;
    double endx;
    double endy;
    int startid;
    int endid;
    int type; // NOTE: map to 'rclass' in database
    int traffic_flow;
};

class DatabaseManager {

    public:
        DatabaseManager();
        virtual ~DatabaseManager();

        int connect(const char* host, const char* user,
                    const char* passwd);
        int query(const char* sql);
        int get_map_data(void);

        inline std::vector<RawData*>& get_raw_data() {
            return raw_data_;
        }

        inline std::vector<DedupedData*>& get_deduped_data() {
            return deduped_data_;
        }

    private:
        void store_raw_data(MYSQL_ROW& row);
        void dedup_raw_data(MYSQL_ROW& row);

        MYSQL* conn_;
        std::vector<RawData*> raw_data_;
        std::vector<DedupedData*> deduped_data_;

        static const char* db_name_;
        static const char* table_name_;
};

#endif // GEOSVR_DATABASE_MANAGER_H_
