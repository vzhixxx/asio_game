#include "app.h"
#include "game_db.h"


namespace app
{

Application::Application(model::Game &game, db::ConnectionPool & connection_pool)
           : game_(game)
           , connection_pool_(connection_pool)
{
    {
        auto conn_wrp = connection_pool.GetConnection();
        db::CreateGameTable(*conn_wrp);
    }

    connection_pool.PrepareQuery(db::TAG_SELECT_RECORDS, db::QUERY_SELECT_RECORDS);
    connection_pool.PrepareQuery(db::TAG_INSERT_PLAYER, db::QUERY_INSERT_PLAYER);
}

void Application::ProcessGameTick(int64_t elapsedMs)
{
    auto retired_players = game_.Think(elapsedMs);

    if (!retired_players.empty()) {
        auto conn_wrp = connection_pool_.GetConnection();
        for (const model::Token * token : retired_players) {
            if (auto p = game_.FindPlayerByToken(*token); p) {

                db::InertRetiredPlayer(*conn_wrp,
                                       p->GetName(),
                                       p->GetDog()->GetScore(),
                                       p->GetPlayingTimeMs());

                game_.RemovePlayerByToken(*token);
            }
        }
    }
}

}