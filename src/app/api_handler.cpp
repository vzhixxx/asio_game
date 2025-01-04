#include "api_handler.h"
#include "request_handler.h"
#include "game_db.h"


using namespace std::string_view_literals;
using namespace std::literals;
using namespace http_handler;
using namespace boost;


namespace api_handler
{
    constexpr auto API_V1 = "/api/v1/"sv;
    constexpr auto AUTH_BEARER = "Bearer "sv;

    namespace api_v1 {
        constexpr auto CMD_GAME_JOIN= "game/join"sv;
        constexpr auto CMD_GAME_PLAYERS = "game/players"sv;
        constexpr auto GAME_PLAYER = "game/player/"sv;
        constexpr auto CMD_GAME_STATE = "game/state"sv;
        constexpr auto CMD_GAME_TICK = "game/tick"sv;
        constexpr auto CMD_GAME_RECORDS = "game/records"sv;
        constexpr auto MAPS = "maps"sv;
        constexpr auto GAME = "game/"sv;
        constexpr auto CMD_GAME_PLAYER_ACTION = "game/player/action"sv;
    }
    

    ApiHandler::ApiHandler(model::Game &game, db::ConnectionPool & connection_pool)
              : connection_pool_(connection_pool)
    {
        app_ = std::make_unique<app::Application>(game, connection_pool);
    }

    StringResponse ApiHandler::HandleApiMapsRequest(std::string_view target, unsigned version, bool keep_alive) const
    {
        StringResponse res;

        target.remove_prefix(api_v1::MAPS.size());

        if (target.empty())
            res = OnCmdMaps(version, keep_alive);
        else if (target.starts_with('/')) {
            target.remove_prefix(1);
            res = OnCmdFetchMap(target, version, keep_alive);
        }
        else
            ThrowBadRequest(version, keep_alive, "Bad request"sv);

        return res;
    }

    StringResponse ApiHandler::OnGameJoin(std::string_view content_type,
                                          std::string_view body,
                                          unsigned version,
                                          bool keep_alive)
    {
        StringResponse ret;

        auto parsed_json = ParseJson(content_type, body, version, keep_alive);

        json::object &objJson = parsed_json.as_object();
        if (objJson.contains("userName") && objJson.contains("mapId"))
        {
            std::string_view userName = objJson["userName"].as_string();
            std::string_view mapId = objJson["mapId"].as_string();

            if (userName.empty())
                ThrowInvalidArgument(version, keep_alive, "Invalid name (empty)"sv);
            else if (auto pMap = app_->FindMap(model::Map::Id{std::string(mapId)}); pMap)
            {
                auto pPlayer = app_->JoinGame(userName, *pMap);
                json::object reply;
                reply["authToken"] = pPlayer->GetToken();
                reply["playerId"]  = pPlayer->GetId();
                ret = MakeStringResponse(http::status::ok,
                                         json::serialize(reply),
                                         version, keep_alive, ContentType::APP_JSON);
            }
            else
                ThrowNotFound(version, keep_alive, "Map not found"sv, "mapNotFound"sv);
        }
        else
            ThrowInvalidArgument(version, keep_alive, "Join game request parse error: userName and mapId"sv);

        return ret;
    }

    StringResponse ApiHandler::OnGamePlayers([[maybe_unused]] std::string_view body,
                                             std::string_view authorization,
                                             unsigned version,
                                             bool keep_alive) const
    {
        const auto &game_player = FindPlayerByToken(authorization, version, keep_alive);

        json::object reply;

        app_->ForEachPlayerOnMap(game_player.GetAssignedMapId(), [&reply](model::Player &player)
                                 {
        json::object j;
        j["name"sv] = player.GetName();
        reply[std::to_string(player.GetId())] = std::move(j); });

        return MakeStringResponse(http::status::ok,
                                  json::serialize(reply),
                                  version, keep_alive, ContentType::APP_JSON);
    }

    StringResponse ApiHandler::OnGamePlayer(std::string_view target,
                                            std::string_view content_type,
                                            std::string_view body,
                                            std::string_view authorization,
                                            unsigned version,
                                            bool keep_alive) const
    {
        StringResponse ret;

        if (target == api_v1::CMD_GAME_PLAYER_ACTION)
        {
            const auto &game_player = FindPlayerByToken(authorization, version, keep_alive);
            if (auto json_req = ParseJson(content_type, body, version, keep_alive); json_req.is_object())
            {
                if (auto it = json_req.as_object().find("move"sv); it != json_req.as_object().end())
                {
                    float fSpeed = app_->GetDefaultDogSpeed();
                    if (auto speed = game_player.GetAssignedMap().GetDogSpeed(); speed)
                        fSpeed = *speed;

                    game_player.GetDog()->SetDirectionCode(it->value().as_string(), fSpeed);

                    ret = MakeStringResponse(http::status::ok,
                                             "{}"sv,
                                             version, keep_alive, ContentType::APP_JSON);
                }
                else
                    ThrowInvalidArgument(version, keep_alive, "Invalid JSON"sv);
            }
            else
                ThrowInvalidArgument(version, keep_alive, "Invalid JSON"sv);
        }
        else
            ThrowBadRequest(version, keep_alive, "Unknown game/player command"sv);

        return ret;
    }

    StringResponse ApiHandler::OnGameTick(std::string_view content_type,
                                          std::string_view body,
                                          unsigned version,
                                          bool keep_alive) const
    {
        StringResponse ret;

        if (!app_->IsAllowedExternalGameTick())
            ThrowBadRequest(version, keep_alive, "Invalid endpoint");

        if (auto json_req = ParseJson(content_type, body, version, keep_alive); json_req.is_object())
        {
            if (auto it = json_req.as_object().find("timeDelta"sv);
                it != json_req.as_object().end() && it->value().is_number())
            {
                int64_t elapsedMs = 0;

                if (it->value().is_int64())
                    elapsedMs = it->value().as_int64();
                else if (it->value().is_uint64())
                    elapsedMs = (int64_t)it->value().as_uint64();
                else if (it->value().is_double())
                    elapsedMs = (int64_t)it->value().as_double();

                if (elapsedMs > 0)
                {
                    app_->ProcessGameTick(elapsedMs);

                    ret = MakeStringResponse(http::status::ok,
                                             "{}"sv,
                                             version, keep_alive, ContentType::APP_JSON);
                }
                else
                    ThrowInvalidArgument(version , keep_alive, "Invalid timeDelta value (zero)"sv);
            }
            else
                ThrowInvalidArgument(version , keep_alive, "Invalid timeDelta value (json)"sv);
        }
        else
            ThrowInvalidArgument(version , keep_alive, "Invalid game/tick JSON"sv);

        return ret;
    }

    StringResponse ApiHandler::OnGameState(std::string_view authorization, unsigned version, bool keep_alive) const
    {
        const auto &game_player = FindPlayerByToken(authorization, version, keep_alive);
        json::object reply;
        reply["players"] = json::object();

        app_->ForEachPlayerOnMap(game_player.GetAssignedMapId(), [&reply](model::Player &player) {
            if (auto pDog = player.GetDog(); pDog) {
                auto         pos = pDog->GetPosition();
                const auto & vel = pDog->GetVelocity();

                json::object data;
                data["pos"]   = json::array( { pos.x, pos.y } );
                data["speed"] = json::array( { vel.x, vel.y } );
                data["dir"]   = pDog->GetDirectionCode();

                json::array bag;
                for (const auto & p : pDog->GetGatheredItems()) {
                    json::object loot;
                    loot["id"] = p->id;
                    loot["type"] = p->type;

                    bag.push_back(std::move(loot));
                }
                data["bag"] = std::move(bag);
                data["score"] = pDog->GetScore();

                reply["players"].as_object().insert_or_assign(std::to_string(player.GetId()),
                                                              std::move(data));
            }
        });

        auto & session = game_player.GetGameSession();
        if (!session.GetLootInstances().empty()) {
            reply["lostObjects"] = json::object();
            auto & lostObjects = reply["lostObjects"].as_object();

            for (const auto & itLoot : session.GetLootInstances()) {

                json::object data;
                data["type"sv] = itLoot->type;
                data["pos"sv] = json::array( { itLoot->pos.x, itLoot->pos.y } );

                lostObjects.insert_or_assign(std::to_string(itLoot->id), std::move(data));
            }
        }

        return MakeStringResponse(http::status::ok,
                                  json::serialize(reply),
                                  version, keep_alive, ContentType::APP_JSON);
    }

    StringResponse ApiHandler::OnGameRecords(std::string_view authorization,
                                             std::string_view body,
                                             unsigned version,
                                             bool keep_alive) const
    {
        const auto &game_player = FindPlayerByToken(authorization, version, keep_alive);
        auto req_json = json::parse(body);
        const auto & req_params = req_json.as_object();

        uint64_t offset = 0;
        uint64_t maxItems = db::MAX_NUM_RECORD_ITEMS;

        if (auto it = req_params.find("start"sv); it != req_params.end() && it->value().is_number())
            offset = it->value().as_uint64();

        if (auto it = req_params.find("maxItems"sv); it != req_params.end() && it->value().is_number())
            maxItems = it->value().as_uint64();

        if (maxItems <= db::MAX_NUM_RECORD_ITEMS) {
            json::array reply;

            auto conn_wrp = connection_pool_.GetConnection();
            auto records = db::FetchRecords(*conn_wrp, offset, maxItems);
            for (const auto & item : records) {
                json::object p;
                p["name"sv] = item.name;
                p["score"sv] = item.score;
                p["playTime"] = item.playtime;

                reply.push_back(std::move(p));
            }

            return MakeStringResponse(http::status::ok,
                                      json::serialize(reply),
                                      version, keep_alive, ContentType::APP_JSON);
        }
        else {
            ThrowInvalidArgument(version, keep_alive, "maxItems is too big"sv);
        }

        return { };
    }

    StringResponse ApiHandler::HandleApiGameRequest(http::verb method,
                                                    std::string_view target,
                                                    std::string_view content_type,
                                                    std::string_view body,
                                                    std::string_view authorization,
                                                    unsigned version,
                                                    bool keep_alive)
    {
        StringResponse ret;

        if (target == api_v1::CMD_GAME_JOIN)
        {
            if (method == http::verb::post)
                ret = OnGameJoin(content_type, body, version, keep_alive);
            else
                ThrowPostOnlyExpected(version, keep_alive);
        }
        else if (target == api_v1::CMD_GAME_PLAYERS)
        {
            if (method == http::verb::get || method == http::verb::head)
                ret = OnGamePlayers(body, authorization, version, keep_alive);
            else
                ThrowGetHeadOnlyExpected(version, keep_alive);
        }
        else if (target == api_v1::CMD_GAME_STATE)
        {
            if (method == http::verb::get || method == http::verb::head)
                ret = OnGameState(authorization, version, keep_alive);
            else
                ThrowGetHeadOnlyExpected(version, keep_alive);
        }
        else if (target == api_v1::CMD_GAME_RECORDS)
        {
            if (method == http::verb::get || method == http::verb::head)
                ret = OnGameRecords(authorization, body, version, keep_alive);
            else
                ThrowGetHeadOnlyExpected(version, keep_alive);
        }
        else if (target.starts_with(api_v1::GAME_PLAYER))
        {
            if (method == http::verb::post)
                ret = OnGamePlayer(target, content_type, body, authorization, version, keep_alive);
            else
                ThrowPostOnlyExpected(version, keep_alive);
        }
        else if (target == api_v1::CMD_GAME_TICK)
        {
            if (method == http::verb::post)
                ret = OnGameTick(content_type, body, version, keep_alive);
            else
                ThrowPostOnlyExpected(version, keep_alive);
        }
        else
        {
            ret = MakeStringResponse(http::status::not_implemented,
                                     "Unknown game command",
                                     version, keep_alive, ContentType::TEXT_HTML);
        }

        return ret;
    }

    StringResponse ApiHandler::HandleApiRequestV1(http::verb method,
                                                  std::string_view target,
                                                  std::string_view content_type,
                                                  std::string_view body,
                                                  std::string_view authorization,
                                                  unsigned version,
                                                  bool keep_alive)
    {
        StringResponse ret;

        if (target.starts_with(api_v1::MAPS)) {
            if (http::verb::get == method || http::verb::head == method)
                ret = HandleApiMapsRequest(target, version, keep_alive);
            else
                ThrowGetHeadOnlyExpected(version, keep_alive);
        }
        else if (target.starts_with(api_v1::GAME))
            ret = HandleApiGameRequest(method, target, content_type, body, authorization, version, keep_alive);
        else
            ThrowMethodNotAllowed(version, keep_alive, "Invalid HTTP method"sv, "");

        ret.set(http::field::cache_control, "no-cache"sv);

        return ret;
    }

    StringResponse ApiHandler::HandleApiRequest(http::verb method,
                                                std::string_view target,
                                                std::string_view content_type,
                                                std::string_view body,
                                                std::string_view authorization,
                                                unsigned version,
                                                bool keep_alive)
    {
        StringResponse res;

        try
        {
            if (target.starts_with(API_V1)) {
                target.remove_prefix(API_V1.size());
                res = HandleApiRequestV1(method, target, content_type, body, authorization, version, keep_alive);
            }
            else
                ThrowBadRequest(version, keep_alive, "Invalid REST API version"sv);
        }
        catch (const ApiHandlerException & err)
        {
            res = err.GetStringResponse();
        }

        return res;
    }

    StringResponse ApiHandler::OnCmdMaps(unsigned version, bool keep_alive) const
    {
        json::array jsonMaps;

        for (const auto &map : app_->GetMaps())
        {
            json::object jsonMap;

            jsonMap["id"] = *map.GetId();
            jsonMap["name"] = map.GetName();

            jsonMaps.push_back(std::move(jsonMap));
        }

        return MakeStringResponse(http::status::ok,
                                  json::serialize(jsonMaps),
                                  version, keep_alive, ContentType::APP_JSON);
    }

    StringResponse ApiHandler::OnCmdFetchMap(std::string_view mapName, unsigned version, bool keep_alive) const
    {
        StringResponse ret;

        std::string sIdMap{mapName};
        if (auto pMap = app_->FindMap(model::Map::Id(sIdMap)); pMap)
        {
            json::object jsonMap;

            jsonMap["id"] = *pMap->GetId();
            jsonMap["name"] = pMap->GetName();

            if (const auto &roads = pMap->GetRoads(); !roads.empty())
            {
                json::array jsonRoads;

                for (const auto &road : roads)
                {
                    json::object jr;

                    auto pos0 = road.GetStart();

                    jr["x0"] = pos0.x;
                    jr["y0"] = pos0.y;

                    if (road.IsHorizontal())
                        jr["x1"] = road.GetEnd().x;
                    else
                        jr["y1"] = road.GetEnd().y;

                    jsonRoads.push_back(std::move(jr));
                }

                jsonMap["roads"] = std::move(jsonRoads);
            }

            if (const auto &buildings = pMap->GetBuildings(); !buildings.empty())
            {
                json::array jsonBuildings;

                for (const auto &building : buildings)
                {
                    json::object jb;

                    const auto &bounds = building.GetBounds();

                    jb["x"] = bounds.position.x;
                    jb["y"] = bounds.position.y;
                    jb["w"] = bounds.size.width;
                    jb["h"] = bounds.size.height;

                    jsonBuildings.push_back(std::move(jb));
                }

                jsonMap["buildings"] = std::move(jsonBuildings);
            }

            if (const auto &offices = pMap->GetOffices(); !offices.empty())
            {
                json::array jsonOffices;

                for (const auto &office : offices)
                {
                    json::object jo;

                    jo["id"]      = *office.GetId();
                    jo["x"]       = office.GetPosition().x;
                    jo["y"]       = office.GetPosition().y;
                    jo["offsetX"] = office.GetOffset().dx;
                    jo["offsetY"] = office.GetOffset().dy;

                    jsonOffices.push_back(std::move(jo));
                }

                jsonMap["offices"] = std::move(jsonOffices);
            }

            if (const auto & lootTypes = pMap->GetLootTypes(); !lootTypes.empty())
            {
                json::array jsonLootTypes;

                for (const auto & lootType : lootTypes)
                {
                    json::object jlt;

                    jlt["name"]     = lootType.name;
                    jlt["file"]     = lootType.file;
                    jlt["type"]     = lootType.type;

                    if (lootType.rotation)
                        jlt["rotation"] = *lootType.rotation;

                    if (lootType.color)
                        jlt["color"] = *lootType.color;

                    if (lootType.scale)
                        jlt["scale"] = *lootType.scale;

                    if (lootType.value)
                        jlt["value"] = *lootType.value;

                    jsonLootTypes.push_back(std::move(jlt));
                }

                jsonMap["lootTypes"] = std::move(jsonLootTypes);
            }

            ret = MakeStringResponse(http::status::ok,
                                     json::serialize(jsonMap),
                                     version, keep_alive,
                                     ContentType::APP_JSON);
        }
        else
            ThrowNotFound(version, keep_alive, "Map not found"sv, "mapNotFound"sv);

        return ret;
    }

    model::Player & ApiHandler::FindPlayerByToken(std::string_view authorization,
                                                  unsigned version,
                                                  bool keep_alive) const
    {
        if (authorization.starts_with(AUTH_BEARER) && authorization.size() == AUTH_BEARER.size() + model::TOKEN_HEX_LENGTH)
        {
            authorization.remove_prefix(AUTH_BEARER.size());

            model::Token token{authorization};

            if (auto pPlayer = app_->FindPlayerByToken(token); pPlayer)
                return *pPlayer;
            else
            {
                json::object jsonErr;
                jsonErr["code"] = "unknownToken"sv;
                jsonErr["message"] = "Unknown token: \'"s + token + '\'';

                auto err = MakeStringResponse(http::status::unauthorized,
                                              json::serialize(jsonErr),
                                              version, keep_alive,
                                              ContentType::APP_JSON);
                throw ApiHandlerException{ std::move(err) };
            }
        }
        else
        {
            json::object jsonErr;
            jsonErr["code"sv]    = "invalidToken"sv;
            jsonErr["message"sv] = "Authorization header is missing"sv;

            auto rsp = MakeStringResponse(http::status::unauthorized,
                                          json::serialize(jsonErr),
                                          version, keep_alive, ContentType::APP_JSON);
            throw ApiHandlerException{ std::move(rsp) };
        }
    }

    json::value ApiHandler::ParseJson(std::string_view content_type, std::string_view body, unsigned version, bool keep_alive)
    {
        json::value ret;

        if (content_type == ContentType::APP_JSON)
        {
            try {
                ret = json::parse(body);
            } catch (const std::exception &e) {
                ThrowInvalidArgument(version, keep_alive, "Parse json error: "s + e.what());
            }
        }
        else
            ThrowInvalidArgument(version, keep_alive, "Request content type error. Only application/json allowed"sv);

        return ret;
    }

    void ApiHandler::ThrowMethodNotAllowed(unsigned version, bool keep_alive, std::string_view message, std::string_view allow, std::string_view code)
    {
        json::object err;
        err["code"sv]    = code;
        err["message"sv] = message;

        auto obj = MakeStringResponse(http::status::method_not_allowed,
                                                 json::serialize(err),
                                                 version,
                                                 keep_alive,
                                                 ContentType::APP_JSON);
        obj.set(http::field::allow, allow);
        obj.set(http::field::cache_control, "no-cache"sv);

        throw ApiHandlerException{ std::move(obj) };
    }

    void ApiHandler::ThrowPostOnlyExpected(unsigned version, bool keep_alive)
    {
        ThrowMethodNotAllowed(version, keep_alive, "Only POST method is expected"sv, "POST"sv);
    }

    void ApiHandler::ThrowGetHeadOnlyExpected(unsigned version, bool keep_alive)
    {
        ThrowMethodNotAllowed(version, keep_alive, "Only GET, HEAD method is expected"sv, "GET, HEAD"sv);
    }

    void ApiHandler::ThrowInvalidArgument(unsigned version, bool keep_alive, std::string_view message)
    {
        ThrowBadRequest(version, keep_alive, message, "invalidArgument"sv);
    }

    void ApiHandler::ThrowBadRequest(unsigned version, bool keep_alive, std::string_view message, std::string_view code)
    {
        json::object err;
        err["code"sv]    = code;
        err["message"sv] = message;

        auto obj = MakeStringResponse(http::status::bad_request,
                                      json::serialize(err),
                                      version, keep_alive, ContentType::APP_JSON);
        obj.set(http::field::cache_control, "no-cache"sv);
        throw ApiHandlerException{ std::move(obj) };
    }

    void ApiHandler::ThrowNotFound(unsigned version, bool keep_alive, std::string_view message, std::string_view code)
    {
        json::object err;
        err["code"sv]    = code;
        err["message"sv] = message;

        auto obj = MakeStringResponse(http::status::not_found,
                                      json::serialize(err),
                                      version, keep_alive, ContentType::APP_JSON);
        obj.set(http::field::cache_control, "no-cache"sv);
        throw ApiHandlerException{ std::move(obj) };
    }
}
