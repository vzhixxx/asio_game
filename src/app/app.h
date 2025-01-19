#pragma once

#include "../lib/model.h"
#include "connection_pool.h"


namespace app
{

class Application
{
public:

    Application(model::Game & game, db::ConnectionPool * connection_pool);

    [[nodiscard]] model::Map * FindMap(const model::Map::Id & id) const noexcept {
        return game_.FindMap(id);
    }

    model::Player * JoinGame(std::string_view user_name, model::Map & map) {
        return game_.Join(user_name, map);
    }

    [[nodiscard]] model::Player * FindPlayerByToken(const model::Token & t) const noexcept {
        return game_.FindPlayerByToken(t);
    }

    void ForEachPlayerOnMap(const model::Map::Id & mapId, const model::PlayerVisitor & pv) const {
        game_.ForEachPlayerOnMap(mapId, pv);
    }

    [[nodiscard]] const auto & GetMaps() const noexcept {
        return game_.GetMaps();
    }

    [[nodiscard]] float GetDefaultDogSpeed() const noexcept {
        return game_.GetDefaultDogSpeed();
    }

    [[nodiscard]] bool IsAllowedExternalGameTick() const noexcept {
        return game_.GetTickPeriod() <= 0;
    }

    void ProcessGameTick(int64_t elapsedMs);

private:

    model::Game & game_;
    db::ConnectionPool * connection_pool_ = nullptr;

};

}   // namespace app