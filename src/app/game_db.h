#pragma once

#include <pqxx/connection>

namespace db
{
    constexpr size_t MAX_NUM_RECORD_ITEMS = 100;
    extern pqxx::zview TAG_SELECT_RECORDS;
    extern pqxx::zview QUERY_SELECT_RECORDS;

    extern pqxx::zview TAG_INSERT_PLAYER;
    extern pqxx::zview QUERY_INSERT_PLAYER;

    struct RecordItem
    {
        std::string name;
        int score;
        float playtime;
    };

    using RecordItems = std::vector<RecordItem>;

    bool CreateGameTable(pqxx::connection & connection);

    bool InertRetiredPlayer(pqxx::connection & connection, std::string_view name, int score, int64_t play_time_ms);

    RecordItems FetchRecords(pqxx::connection & connection, int start, int maxItems);



}
