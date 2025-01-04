#pragma once

#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <functional>
#include <optional>

#include "tagged.h"
#include "glm_include.h"
#include "loot_generator.h"
#include "collision_detector.h"


namespace model
{

constexpr size_t TOKEN_HEX_LENGTH = 32;

using Dimension         = int;
using Coord             = Dimension;
using Bounds            = std::pair<glm::vec2, glm::vec2>;
using Token             = std::string;
using LootInstancePtr   = std::shared_ptr<loot_gen::LootInstance>;
using LootInstances     = std::vector<LootInstancePtr>;
using LootTypes         = std::vector<loot_gen::LootType>;

using Clock             = std::chrono::steady_clock;
using TimePoint         = Clock::time_point;
namespace cd            = collision_detector;


struct Point
{
    Coord x;
    Coord y;
};


struct Size
{
    Dimension width;
    Dimension height;
};


struct Rectangle
{
    Point   position;
    Size    size;
};


struct Offset
{
    Dimension dx;
    Dimension dy;
};


enum class Direction : char
{
    North = 'U',
    South = 'D',
    West  = 'L',
    East  = 'R'
};


class Road
{
    struct HorizontalTag { };
    struct VerticalTag   { };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag   VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_  {end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_  {start.x, end_y} {
    }

    [[nodiscard]] bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    [[nodiscard]] bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    [[nodiscard]] Point GetStart() const noexcept {
        return start_;
    }

    [[nodiscard]] Point GetEnd() const noexcept {
        return end_;
    }

    [[nodiscard]] bool IsOnTheRoad(const glm::vec2 & pos) const;

    [[nodiscard]] glm::vec2 BoundToTheRoad(const glm::vec2 & pos) const;

    [[nodiscard]] Bounds GetBounds() const;

private:
    Point start_;
    Point end_;
};


class Building
{
public:
    explicit Building(const Rectangle & bounds) noexcept
        : bounds_{bounds} {
    }

    [[nodiscard]] const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};


class Office
{
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_      {std::move(id)}
        , position_{position}
        , offset_  {offset} {
    }

    [[nodiscard]] const Id& GetId() const noexcept {
        return id_;
    }

    [[nodiscard]] Point GetPosition() const noexcept {
        return position_;
    }

    [[nodiscard]] Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};


class Dog
{
public:

    using Id = std::uint64_t;

    Dog();

    [[nodiscard]] Id GetId() const noexcept { return id_; }

    [[nodiscard]] const auto & GetPosition() const noexcept {
        return pos_;
    }

    void SetPosition(const glm::dvec2 & pos);

    [[nodiscard]] const auto & GetPrevPosition() const noexcept {
        return prev_pos_;
    }

    [[nodiscard]] const auto & GetVelocity() const noexcept {
        return velocity_;
    }

    [[nodiscard]] std::string GetDirectionCode() const noexcept;

    void SetDirectionCode(std::string_view d, float speed);

    bool GatherItem(const LootInstancePtr & item, size_t maxBagCapacity);

    [[nodiscard]] const auto & GetGatheredItems() const noexcept {
        return gathered_items_;
    }

    void StoreLootsAtOffice(const LootTypes & lootTypes) noexcept;

    [[nodiscard]] int GetScore() const noexcept {
        return score_;
    }

    [[nodiscard]] bool IsStopped() const noexcept;

private:

    std::uint64_t id_ = 0;
    std::string nickname_;
    glm::dvec2 pos_ = { };
    glm::dvec2 velocity_ = { };
    std::optional<glm::dvec2> prev_pos_ = std::nullopt;
    Direction direction_ = Direction::North;
    LootInstances gathered_items_;
    int score_ = 0;
    TimePoint creation_time_ = Clock::now();
};


class Map
{
public:
    using Id               = util::Tagged<std::string, Map>;
    using Roads            = std::vector<Road>;
    using Buildings        = std::vector<Building>;
    using Offices          = std::vector<Office>;

    Map(Id id, std::string  name);

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(const Office & office);

    void AddLootType(loot_gen::LootType && loot) {
        lootTypes_.push_back(std::move(loot));
    }

    const auto & GetLootTypes () const noexcept {
        return lootTypes_;
    }

    void SetDogSpeed(float f) {
        dogSpeed_ = f;
    }

    auto GetDogSpeed() const noexcept {
        return dogSpeed_;
    }

    void SetBagCapacity(size_t bagCapacity) {
        bagCapacity_ = bagCapacity;
    }

    auto GetBagCapacity() const noexcept {
        return bagCapacity_;
    }

    [[maybe_unused]] bool IsPositionOnRoad(const glm::vec2 & pos) const;

    std::optional<glm::dvec2> BoundedMove(const glm::dvec2 & origin, const glm::dvec2 & newPos) const;

    glm::dvec2 GenerateRandomPositionOnRoad() const;

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    std::optional<float> dogSpeed_ = std::nullopt;
    std::optional<size_t> bagCapacity_ = std::nullopt;

    LootTypes lootTypes_;
};


class GameSession : public collision_detector::ItemGathererProvider
{
    using Dogs             = std::vector<std::unique_ptr<Dog> >;
    using LootGeneratorPtr = std::unique_ptr<loot_gen::LootGenerator>;

public:

    using Id = std::uint64_t;

    GameSession(Map & map, const loot_gen::LootGeneratorConfig & cfg, double dogRetirementTime);

    [[nodiscard]] Id GetId() const noexcept {
        return id_;
    }

    [[nodiscard]] const Map & GetMap() const noexcept {
        return map_;
    }

    [[nodiscard]] const auto & GetLootInstances() const noexcept {
        return lootInstances_;
    }

    [[nodiscard]] const Map::Id & GetMapIp() const noexcept;

    Dog & AddNewDog();

    const auto & GetDogs() const noexcept {
        return dogs_;
    }

    void Think(int64_t elapsedMs);

private:

    [[nodiscard]] size_t ItemsCount() const override {
        return lootInstances_.size();
    }
    [[nodiscard]] cd::Item GetItem(size_t idx) const override;
    [[nodiscard]] size_t GatherersCount() const override {
        return dogs_.size();
    }
    [[nodiscard]] cd::Gatherer GetGatherer(size_t idx) const override;

    void TryCollectLoots();

    void TryStoreLootsAtOffices();

    double GetDogRetirementTime() const noexcept {
        return dogRetirementTime_;
    }

private:

    Id id_;
    Dogs dogs_;
    Map & map_;
    double dogRetirementTime_ = 60;

    LootInstances lootInstances_;
    LootGeneratorPtr pLootGenerator_;
};


class Player
{
public:

    using Id = uint64_t;

    Player(std::string_view user_name, Token token, GameSession & session, Dog & dog);

    [[nodiscard]] Id GetId() const noexcept {
        return id_;
    }

    [[nodiscard]] const Token & GetToken() const noexcept {
        return token_;
    }

    [[nodiscard]] GameSession & GetGameSession() const noexcept {
        return session_;
    }

    [[nodiscard]] const Map & GetAssignedMap() const noexcept {
        return session_.GetMap();
    }

    [[nodiscard]] const Map::Id & GetAssignedMapId() const noexcept {
        return session_.GetMapIp();
    }

    [[nodiscard]] const std::string & GetName() const noexcept {
        return user_name_;
    }

    [[nodiscard]] Dog * GetDog() const noexcept {
        return &dog_;
    }

    [[nodiscard]] glm::dvec2 GetPosition() const noexcept;
    [[nodiscard]] glm::dvec2 EstimateNewPosition(int64_t elapsedMs) const;

    void Think(int64_t elapsedMs);

    [[nodiscard]] bool IsStopped() const noexcept {
        return dog_.IsStopped();
    }

    [[nodiscard]] int64_t GetPlayingTimeMs() const noexcept {
        return playing_time_ms_;
    }

    bool IsRetired() const;

private:

    std::string user_name_;
    Token token_;
    Id id_;
    GameSession & session_;
    Dog & dog_;
    int64_t playing_time_ms_ = 0;
    int64_t stopped_time_ms_ = 0;
};

using PlayerVisitor = std::function<void(Player & )>;


class PlayerTokens
{
    using Token2Player  = std::unordered_map<Token, std::unique_ptr<Player> >;

public:

    Player * CreatePlayer(std::string_view user_name, GameSession & session, Dog & dog);
    Player * Find(const Token & t) const;
    bool Remove(const Token & t);

    void ForEachPlayerOnMap(const Map::Id & mapId, const PlayerVisitor& pv) const;
    void ForEachPlayer(const PlayerVisitor& pv) const;

private:

    Token generate();


    Token2Player token_to_player_;
    std::random_device random_device_;

    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
};


class Players
{
public:

    Player * CreatePlayer(std::string_view user_name, GameSession & session, Dog & dog);
    Player * FindByToken(const Token & t) const;
    bool RemoveByToken(const Token & t);

    void ForEachPlayerOnMap(const Map::Id & mapId, const PlayerVisitor& pv) const {
        player_tokens_.ForEachPlayerOnMap(mapId, pv);
    }

    void ForEachPlayer(const PlayerVisitor& pv) const {
        player_tokens_.ForEachPlayer(pv);
    }

private:

    PlayerTokens player_tokens_;
};


class Game
{
    using MapIdHasher  = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using Maps         = std::vector<Map>;
    using GameSessions = std::unordered_map<GameSession::Id, std::unique_ptr<GameSession> >;

public:

    using RetiredPlayers = std::vector<const Token *>;

    void AddMap(Map && map);

    const Maps & GetMaps() const noexcept { return maps_; }

    const Map* FindMap(const Map::Id& id) const noexcept;
    Map* FindMap(const Map::Id& id) noexcept;

    [[maybe_unused]] Player * Join(std::string_view userName, const Map::Id & mapId);
    Player * Join(std::string_view userName, Map & map);

    Player * FindPlayerByToken(const Token & t) const;

    bool RemovePlayerByToken(const Token & t);

    void ForEachPlayerOnMap(const Map::Id & mapId, const PlayerVisitor& pv) const {
        players_.ForEachPlayerOnMap(mapId, pv);
    }

    void SetDefaultDogSpeed(float f) {
        defaultDogSpeed_ = f;
    }

    float GetDefaultDogSpeed() const noexcept {
        return defaultDogSpeed_;
    }

    void SetDefaultBagCapacity(size_t sz) {
        defaultBagCapacity_ = sz;
    }

    size_t GetDefaultBagCapacity() const noexcept {
        return defaultBagCapacity_;
    }

    void SetRandomizeSpawnPoints(bool b) noexcept {
        randomize_spawn_points_ = b;
    }

    void SetLootGeneratorConfig(const loot_gen::LootGeneratorConfig & cfg) {
        lootGeneratorCfg_ = cfg;
    }

    const auto & GetLootGeneratorConfig() const {
        return lootGeneratorCfg_;
    }

    void SetTickPeriod(int p) noexcept {
        tick_period_ = p;
    }

    int GetTickPeriod() const noexcept {
        return tick_period_;
    }

    void SetDogRetirementTime(double t) noexcept {
        dogRetirementTime_ = t;
    }

    double GetDogRetirementTime() const noexcept {
        return dogRetirementTime_;
    }

    RetiredPlayers Think(int64_t elapsedMs);

private:

    GameSession & GetOrCreateSession(Map & map);


    Maps maps_;
    MapIdToIndex map_id_to_index_;

    GameSessions sessions_;
    Players players_;

    float defaultDogSpeed_ = 1;
    bool randomize_spawn_points_ = false;
    int tick_period_ = 0;
    size_t defaultBagCapacity_ = 3;
    double dogRetirementTime_ = 60;
    loot_gen::LootGeneratorConfig lootGeneratorCfg_;
};

}  // namespace model
