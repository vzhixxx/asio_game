#include "json_loader.h"

#include <fstream>
#include <iostream>

#include <boost/json.hpp>

using namespace std::literals;
namespace fs = std::filesystem;
using namespace boost;

namespace json_loader
{
    
static void LoadRoads(const json::array & roads, model::Map & gameMap)
{
    for (const auto & itRoad : roads)
    {
        auto x0 = (model::Coord)itRoad.at("x0").as_int64();
        auto y0 = (model::Coord)itRoad.at("y0").as_int64();

        if (auto pX1 = itRoad.as_object().if_contains("x1"); pX1)
        {
            auto x1 = (model::Coord)pX1->as_int64();

            model::Road road(model::Road::HORIZONTAL, { x0, y0 }, x1);
            gameMap.AddRoad(road);
        }
        else if (auto pY1 = itRoad.as_object().if_contains("y1"); pY1)
        {
            auto y1 = (model::Coord)pY1->as_int64();

            model::Road road(model::Road::VERTICAL, { x0, y0 }, y1);
            gameMap.AddRoad(road);
        }
    }
}

static void LoadBuildings(const json::array & buildings, model::Map & gameMap)
{
    for (const auto & building : buildings)
    {
        model::Rectangle r = { };
    
        r.position.x  = (model::Coord)building.at("x").as_int64();
        r.position.y  = (model::Coord)building.at("y").as_int64();
        r.size.width  = (model::Coord)building.at("w").as_int64();
        r.size.height = (model::Coord)building.at("h").as_int64();
    
        gameMap.AddBuilding(model::Building(r));
    }
}

static void LoadOffices(const json::array & offices, model::Map & gameMap)
{
    for (const auto & itOffice : offices)
    {
        model::Point position = { };
        model::Offset offset  = { };
    
        std::string id =                   itOffice.at("id").as_string().c_str();
        position.x     = (model::Coord)    itOffice.at("x").as_int64();
        position.y     = (model::Coord)    itOffice.at("y").as_int64();
        offset.dx      = (model::Dimension)itOffice.at("offsetX").as_int64();
        offset.dy      = (model::Dimension)itOffice.at("offsetY").as_int64();
    
        model::Office office(model::Office::Id{id}, position, offset);
        gameMap.AddOffice(office);
    }
}

static void LoadLootTypes(const json::array & lootTypes, model::Map & gameMap)
{
    for (const auto & itLoot : lootTypes)
    {
        loot_gen::LootType lootType;

        lootType.name     = itLoot.at("name"sv).as_string();
        lootType.file     = itLoot.at("file"sv).as_string();
        lootType.type     = itLoot.at("type"sv).as_string();

        if (auto itRot = itLoot.as_object().find("rotation"sv);
            itRot != itLoot.as_object().end() && itRot->value().is_number()) {
            lootType.rotation = itRot->value().as_int64();
        }

        if (auto itClr = itLoot.as_object().find("color"sv);
            itClr != itLoot.as_object().end() && itClr->value().is_string()) {
            lootType.color = itClr->value().as_string();
        }

        if (auto itScl = itLoot.as_object().find("scale"sv);
            itScl != itLoot.as_object().end() && itScl->value().is_number()) {
            lootType.scale = itScl->value().as_double();
        }

        if (auto itValue = itLoot.as_object().find("value"sv);
            itValue != itLoot.as_object().end() && itValue->value().is_number()) {
            lootType.value = itValue->value().as_int64();
        }

        gameMap.AddLootType(std::move(lootType));
    }
}

static void ParseLootGeneratorConfig(model::Game & game, const json::object & json)
{
    loot_gen::LootGeneratorConfig cfg;

    if (auto it = json.find("period"sv);
        it != json.end() && it->value().is_double()) {
        cfg.period = it->value().as_double();
    }

    if (auto it = json.find("probability"sv);
            it != json.end() && it->value().is_double()) {
        cfg.probability = it->value().as_double();
    }

    game.SetLootGeneratorConfig(cfg);
}

std::unique_ptr<model::Game> LoadGame(const fs::path & jsonPath)
{
    auto pGame = std::make_unique<model::Game>();

    if (std::ifstream file(jsonPath); file) {
        std::string strJson((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

        auto jsonRoot = json::parse(strJson);
        if (auto it = jsonRoot.as_object().find("defaultDogSpeed"sv);
            it != jsonRoot.as_object().end() && it->value().is_double()) {
            pGame->SetDefaultDogSpeed((float)it->value().as_double());
        }

        if (auto it = jsonRoot.as_object().find("defaultBagCapacity"sv);
            it != jsonRoot.as_object().end() && it->value().is_number()) {
            size_t defaultBagCapacity = 3;
            if (it->value().is_uint64())
                defaultBagCapacity = it->value().as_uint64();
            else if (it->value().is_int64())
                defaultBagCapacity = it->value().as_int64();

            pGame->SetDefaultBagCapacity(defaultBagCapacity);
        }

        if (auto it = jsonRoot.as_object().find("lootGeneratorConfig"sv);
            it != jsonRoot.as_object().end() && it->value().is_object()) {
            ParseLootGeneratorConfig(*pGame, it->value().as_object());
        }

        if (auto it = jsonRoot.as_object().find("dogRetirementTime"sv);
            it != jsonRoot.as_object().end() && it->value().is_number()) {
            double dogRetirementTime = 60.0;
            if (it->value().is_double())
                dogRetirementTime = it->value().as_double();
            else if (it->value().is_int64())
                dogRetirementTime = static_cast<double>(it->value().as_int64());
            else if (it->value().is_uint64())
                dogRetirementTime = static_cast<double>(it->value().as_uint64());

            pGame->SetDogRetirementTime(dogRetirementTime);
        }

        if (const auto & maps = jsonRoot.at("maps"sv); maps.is_array())
        {
            for (const auto & it : maps.as_array())
            {
                const auto & map = it.as_object();

                std::string id   = map.at("id")  .as_string().c_str();
                std::string name = map.at("name").as_string().c_str();

                model::Map gameMap(model::Map::Id{id}, name);

                if (auto itDogSpeed = map.find("dogSpeed"sv);
                    itDogSpeed != map.end() && itDogSpeed->value().is_double()) {
                    gameMap.SetDogSpeed((float) itDogSpeed->value().as_double());
                }

                if (auto itBag = map.find("bagCapacity"sv);
                    itBag != map.end() && itBag->value().is_number()) {
                    size_t bagCapacity = 0;
                    if (itBag->value().is_uint64())
                        bagCapacity = itBag->value().as_uint64();
                    else if (itBag->value().is_int64())
                        bagCapacity = itBag->value().as_int64();

                    gameMap.SetBagCapacity(bagCapacity);
                }

                if (const auto & roads = map.at("roads").as_array(); !roads.empty())
                    LoadRoads(roads, gameMap);

                if (const auto & buildings = map.at("buildings").as_array(); !buildings.empty())
                    LoadBuildings(buildings, gameMap);

                if (const auto & offices = map.at("offices").as_array(); !offices.empty())
                    LoadOffices(offices, gameMap);

                if (const auto & lootTypes = map.at("lootTypes").as_array(); !lootTypes.empty())
                    LoadLootTypes(lootTypes, gameMap);

                pGame->AddMap(std::move(gameMap));
            }
        }
    }
    else
    {
        throw std::invalid_argument("Failed open config json for reading");
    }

    return pGame;
}

}  // namespace json_loader
