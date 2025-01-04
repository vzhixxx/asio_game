#include "game_db.h"

#include <iostream>
#include <pqxx/pqxx>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

using namespace std::literals;
// libpqxx использует zero-terminated символьные литералы вроде "abc"_zv;
using pqxx::operator"" _zv;

namespace db
{

pqxx::zview TAG_SELECT_RECORDS = "sel_records_tag";
pqxx::zview QUERY_SELECT_RECORDS = "SELECT name score play_time_ms FROM retired_players ORDER BY score DESC, play_time_ms, name LIMIT $1 OFFSET $2;";

pqxx::zview TAG_INSERT_PLAYER = "ins_records_tag";
pqxx::zview QUERY_INSERT_PLAYER = "INSERT INTO retired_players VALUES (DEFAULT, $1, $2, $3);";


bool CreateGameTable(pqxx::connection & connection)
{
    bool bRet = false;

    try {
        pqxx::work w(connection);

        w.exec("CREATE TABLE IF NOT EXISTS retired_players  ("
               "id UUID PRIMARY KEY, "
               "name varchar(100) NOT NULL, "
               "score integer NOT NULL, "
               "play_time_ms integer NOT NULL;"_zv);

        w.exec("CREATE INDEX IF NOT EXISTS retired_players_idx ON retired_players ("
               "score DESC, play_time_ms, name);"_zv);

        // Применяем все изменения
        w.commit();

        bRet = true;
    }
    catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    }

    return bRet;
}

bool InertRetiredPlayer(pqxx::connection & connection, std::string_view name, int score, int64_t play_time_ms)
{
    bool bRet = false;

    try {
        pqxx::work w(connection);

        auto result = w.exec_prepared(db::TAG_INSERT_PLAYER, name, score, static_cast<int>(play_time_ms));
        if (!result.empty())
            bRet = true;
    }
    catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    }

    return bRet;
}

RecordItems FetchRecords(pqxx::connection & connection, int start, int maxItems)
{
    RecordItems retItems;

    try {
        pqxx::read_transaction r(connection);

        auto result = r.exec_prepared(db::TAG_SELECT_RECORDS, start, maxItems);
        for (const auto & row : result) {
            const auto & name = row[0];
            const auto & score = row[1];
            const auto & play_time_ms = row[2];

            RecordItem rec;
            rec.name = row[0].as<std::string_view>();
            rec.score = row[1].as<int>();
            rec.playtime = static_cast<float>(row[2].as<int>()) / 1000.0f;

            retItems.push_back(rec);
        }
    }
    catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    }

    return retItems;
}

}