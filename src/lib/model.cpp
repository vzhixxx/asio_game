#include "model.h"

#include <utility>


using namespace loot_gen;
using namespace std::literals;
using namespace glm;


namespace model
{

constexpr float ROAD_WIDTH      = 0.8f;
constexpr float ROAD_HALF_WIDTH = ROAD_WIDTH * 0.5f;


class CMapItemGathererProvider : public cd::ItemGathererProvider
{
public:
    CMapItemGathererProvider(const GameSession & s) : session_(s) {
    }

private:

    [[nodiscard]] size_t ItemsCount() const override {
        return session_.GetMap().GetOffices().size();
    }
    [[nodiscard]] cd::Item GetItem(size_t idx) const override {
        cd::Item ret = { };

        if (idx < ItemsCount()) {
            const auto & office = session_.GetMap().GetOffices().at(idx);
            auto pos = office.GetPosition();
            ret.position = { pos.x, pos.y };
            ret.width = 0.5;
        }

        return ret;
    }
    [[nodiscard]] size_t GatherersCount() const override {
        return session_.GetDogs().size();
    }
    [[nodiscard]] cd::Gatherer GetGatherer(size_t idx) const override {
        cd::Gatherer ret = { };

        if (idx < GatherersCount()) {
            const auto & dog = session_.GetDogs().at(idx);

            ret.end_pos = dog->GetPosition();
            if (auto start_pos = dog->GetPrevPosition(); start_pos)
                ret.start_pos = *start_pos;
            else
                ret.start_pos = ret.end_pos;

            ret.width = 0.6;
        }

        return ret;
    }

private:
    const GameSession & session_;
};


Token PlayerTokens::generate()
{
    uint64_t t1 = generator1_();
    uint64_t t2 = generator2_();
    // Чтобы сгенерировать токен, получите из generator1_ и generator2_
    // два 64-разрядных числа и, переведя их в hex-строки, склейте в одну.
    // Вы можете поэкспериментировать с алгоритмом генерирования токенов,
    // чтобы сделать их подбор ещё более затруднительным

    std::ostringstream ss;
    ss << std::setw(16) << std::setfill('0') << std::hex << t1;
    ss << std::setw(16) << std::setfill('0') << std::hex << t2;

    return ss.str();
}

bool Road::IsOnTheRoad(const glm::vec2 &pos) const
{
    auto [lb, rt] = GetBounds();

    return lb.x <= pos.x && lb.y <= pos.y && rt.x >= pos.x && rt.y >= pos.y;
}

glm::vec2 Road::BoundToTheRoad(const glm::vec2 &pos) const
{
    auto [lb, rt] = GetBounds();
    return
    {
        std::clamp(pos.x, lb.x, rt.x),
        std::clamp(pos.y, lb.y, rt.y)
    };
}

Bounds Road::GetBounds() const
{
    float left_bottom_x = static_cast<float>(std::min(start_.x, end_.x)) - ROAD_HALF_WIDTH;
    float left_bottom_y = static_cast<float>(std::min(start_.y, end_.y)) - ROAD_HALF_WIDTH;
    float right_top_x   = static_cast<float>(std::max(start_.x, end_.x)) + ROAD_HALF_WIDTH;
    float right_top_y   = static_cast<float>(std::max(start_.y, end_.y)) + ROAD_HALF_WIDTH;

    Bounds b =
    {
        { left_bottom_x, left_bottom_y },
        { right_top_x,   right_top_y   }
    };

    return b;
}

Map::Map(Id id, std::string  name)
    : id_(std::move(id))
    , name_(std::move(name))
{
}

void Map::AddOffice(const Office & office)
{
    if (warehouse_id_to_index_.contains(office.GetId()))
    {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(office);
    try
    {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    }
    catch (...)
    {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

bool Map::IsPositionOnRoad(const glm::vec2 &pos) const
{
    // Note: binary search tree optimization here in future

    for (const auto & r : roads_) {
        if (r.IsOnTheRoad(pos))
            return true;
    }

    return false;
}

std::optional<glm::dvec2> Map::BoundedMove(const glm::dvec2 &origin, const glm::dvec2 &newPos) const
{
    std::vector<const Road *> roads;

    for (const Road & r : roads_) {
        if (r.IsOnTheRoad(origin))
            roads.push_back(&r);
    }

    if (roads.empty())
        return std::nullopt;

    glm::dvec2 most_far = roads.front()->BoundToTheRoad(glm::vec2(newPos));
    if (roads.size() > 1)
    {
        // Note: possible optimization length squared
        auto max_distance = glm::distance2(origin, most_far);

        for (size_t i = 1; i < roads.size(); ++i)
        {
            glm::dvec2 pretender = roads[i]->BoundToTheRoad(glm::vec2(newPos));
            auto distance = glm::distance2(origin, pretender);
            if (distance > max_distance)
            {
                most_far     = pretender;
                max_distance = distance;
            }
        }
    }

    return most_far;
}

glm::dvec2 Map::GenerateRandomPositionOnRoad() const
{
    glm::dvec2 pos(0);

    if (!roads_.empty()) {
        std::random_device rd;
        std::mt19937 mt(rd());

        std::uniform_int_distribution<size_t> dist(0, roads_.size() - 1);
        size_t idxRoad = dist(mt);

        const auto & road = roads_[idxRoad];
        auto bounds = road.GetBounds();

        std::uniform_real_distribution<float> distBoundX(bounds.first.x, bounds.second.x);
        std::uniform_real_distribution<float> distBoundY(bounds.first.y, bounds.second.y);

        pos.x = distBoundX(mt);
        pos.y = distBoundY(mt);
    }

    return pos;
}


GameSession::GameSession(Map & map, const loot_gen::LootGeneratorConfig & cfg, double dogRetirementTime)
           : map_(map)
           , dogRetirementTime_(dogRetirementTime)
{
    static Id s_id = 0;
    id_ = ++s_id;

    std::chrono::seconds seconds { cfg.period };
    LootGenerator::TimeInterval interval {seconds };

    pLootGenerator_ = std::make_unique<LootGenerator>(interval,
                                                      cfg.probability);
}

const Map::Id &GameSession::GetMapIp() const noexcept
{
    return map_.GetId();
}

Dog & GameSession::AddNewDog()
{
    auto pDog = std::make_unique<Dog>();
    auto id = pDog->GetId();
    auto & dog = *pDog;

    dogs_.push_back(std::move(pDog));

    return *dogs_.back();
}

void GameSession::Think(int64_t elapsedMs)
{
    size_t numLoots = pLootGenerator_->Generate(LootGenerator::TimeInterval(elapsedMs),
                                                lootInstances_.size(),
                                               dogs_.size());
    if (numLoots > 0 && !map_.GetLootTypes().empty()) {
        std::random_device rd;
        std::mt19937 mt(rd());
        std::uniform_int_distribution<std::mt19937::result_type> dist(0, map_.GetLootTypes().size() - 1);

        static LootInstance::Id s_id = 0;

        for (size_t n = 0; n < numLoots; ++n) {
            auto pLoot = std::make_shared<LootInstance>();
            pLoot->id = ++s_id;
            pLoot->type = static_cast<int>(dist(mt));
            pLoot->pos = map_.GenerateRandomPositionOnRoad();

            lootInstances_.push_back(std::move(pLoot));
        }
    }

    // Note: All players finish moving - try collect loots
    TryCollectLoots();

    TryStoreLootsAtOffices();
}

void GameSession::TryCollectLoots()
{
    size_t bagCapacity = 3;
    if (auto bc = map_.GetBagCapacity(); bc)
        bagCapacity = *bc;

    bool anyGathered = false;

    for (const auto & ge : cd::FindGatherEvents(*this)) {
        const auto & dog = dogs_[ge.gatherer_id];
        const auto & itm = lootInstances_[ge.item_id];

        if (dog->GatherItem(itm, bagCapacity))
            anyGathered = true;
    }

    if (anyGathered) {
        auto it = std::remove_if(lootInstances_.begin(),
                                 lootInstances_.end(),
                                 [](const LootInstancePtr & p) {
            return p->gathered;
        });

        if (it != lootInstances_.end()) {
            lootInstances_.erase(it, lootInstances_.end());
        }
    }
}

void GameSession::TryStoreLootsAtOffices()
{
    CMapItemGathererProvider provider(*this);

    for (const auto & ge : cd::FindGatherEvents(*this)) {
        const auto & dog = dogs_[ge.gatherer_id];
        dog->StoreLootsAtOffice(map_.GetLootTypes());
    }
}

cd::Item GameSession::GetItem(size_t idx) const
{
    cd::Item ret = { };
    if (idx < lootInstances_.size()) {
        const LootInstance & loot = *lootInstances_[idx];
        ret.position = loot.pos;
    }

    return ret;
}

cd::Gatherer GameSession::GetGatherer(size_t idx) const
{
    cd::Gatherer ret = { };
    if (idx < dogs_.size()) {
        const auto & dog = dogs_[idx];

        ret.end_pos = dog->GetPosition();
        if (const auto & start_pos = dog->GetPrevPosition(); start_pos)
            ret.start_pos = *start_pos;
        else
            ret.start_pos = ret.end_pos;

        ret.width = 0.6;
    }

    return ret;
}


Player * PlayerTokens::CreatePlayer(std::string_view user_name, GameSession & session, Dog & dog)
{
    auto p = std::make_unique<Player>(user_name, generate(), session, dog);
    auto pRet = p.get();

    token_to_player_[p->GetToken()] = std::move(p);

    return pRet;
}

Player *PlayerTokens::Find(const Token &t) const
{
    auto it = token_to_player_.find(t);
    return it != token_to_player_.end() ? it->second.get() : nullptr;
}

bool PlayerTokens::Remove(const Token &t)
{
    auto res = token_to_player_.erase(t);
    return res > 0;
}

void PlayerTokens::ForEachPlayerOnMap(const Map::Id &mapId, const PlayerVisitor& pv) const
{
    for (const auto & p : token_to_player_)
    {
        if (p.second->GetAssignedMapId() == mapId)
            pv(*p.second);
    }
}

void PlayerTokens::ForEachPlayer(const PlayerVisitor& pv) const
{
    for (const auto & p : token_to_player_)
        pv(*p.second);
}

Player * Players::CreatePlayer(std::string_view user_name, GameSession & session, Dog & dog)
{
    return player_tokens_.CreatePlayer(user_name, session, dog);
}

Player * Players::FindByToken(const Token &t) const
{
    return player_tokens_.Find(t);
}

bool Players::RemoveByToken(const Token & t)
{
    return player_tokens_.Remove(t);
}

void Game::AddMap(Map && map)
{
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted)
    {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    }
    else
    {
        try
        {
            maps_.push_back(std::move(map));
        }
        catch (...)
        {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

    [[maybe_unused]] const Map * Game::FindMap(const Map::Id& id) const noexcept
{
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end())
        return &maps_[it->second];

    return nullptr;
}

Map * Game::FindMap(const Map::Id &id) noexcept
{
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end())
        return &maps_[it->second];

    return nullptr;
}

Player * Game::Join(std::string_view userName, const model::Map::Id & mapId)
{
    if (auto pMap = FindMap(mapId); pMap)
        return Join(userName, *pMap);

    return nullptr;
}

Player * Game::Join(std::string_view user_name, Map & map)
{
    auto & session = GetOrCreateSession(map);
    auto & dog = session.AddNewDog();

    if (randomize_spawn_points_)
        dog.SetPosition(map.GenerateRandomPositionOnRoad());
    else if (!map.GetRoads().empty()) {
        auto start = map.GetRoads().front().GetStart();
        dog.SetPosition({ start.x, start.y });
    }

    return players_.CreatePlayer(user_name, session, dog);
}

Player * Game::FindPlayerByToken(const Token & t) const
{
    return players_.FindByToken(t);
}

bool Game::RemovePlayerByToken(const Token & t)
{
    return players_.RemoveByToken(t);
}

Game::RetiredPlayers Game::Think(int64_t elapsedMs)
{
    RetiredPlayers ret;
    auto tpNow = Clock::now();
    // Note: Update player moving
    players_.ForEachPlayer([&](Player & player) {
        player.Think(elapsedMs);

        if (player.IsRetired())
            ret.push_back(&player.GetToken());
    });

    for (auto & s : sessions_)
        s.second->Think(elapsedMs);

    return ret;
}

GameSession &Game::GetOrCreateSession(Map &map)
{
    for (const auto & it : sessions_)
    {
        if (it.second->GetMapIp() == map.GetId())
            return *it.second;
    }

    auto pNewSession = std::make_unique<GameSession>(map,
                                                                          GetLootGeneratorConfig(),
                                                                          GetDogRetirementTime());
    auto pRet = pNewSession.get();
    sessions_[pNewSession->GetId()] = std::move(pNewSession);

    return *pRet;
}


Player::Player(std::string_view user_name, Token token, GameSession & session, Dog & dog)
      : user_name_(user_name)
      , token_(std::move(token))
      , session_(session)
      , dog_(dog)
{
    static Id s_id_players = 0;
    id_ = s_id_players++;
}

glm::dvec2 Player::GetPosition() const noexcept
{
    return dog_.GetPosition();
}

glm::dvec2 Player::EstimateNewPosition(int64_t elapsedMs) const
{
    auto pos   = dog_.GetPosition();
    auto shift = dog_.GetVelocity() * (static_cast<double>(elapsedMs) / 1000.0);

    return pos + shift;
}

void Player::Think(int64_t elapsedMs)
{
    auto oldPos = GetPosition();
    auto estimatedNewPos = EstimateNewPosition(elapsedMs);
    auto newPos = session_.GetMap().BoundedMove(oldPos, estimatedNewPos);

    if (newPos)
        dog_.SetPosition(*newPos);

    if (!newPos ||
        glm::any(epsilonNotEqual(vec2(*newPos), vec2(estimatedNewPos), vec2(FLT_EPSILON)))) {
        dog_.SetDirectionCode(""sv, 0);     // Note: Stop dog
    }

    playing_time_ms_ += elapsedMs;
    stopped_time_ms_ = IsStopped() ? stopped_time_ms_ + elapsedMs : 0;
}

bool Player::IsRetired() const
{
    bool bRet = false;
    if (IsStopped()) {
        //static_cast<double>(stopped_time_ms_) / 1000.0 >= session_.GetRe


    }

    return bRet;
}


Dog::Dog()
{
    static Id s_id_dogs = 0;
    id_ = s_id_dogs++;
}

void Dog::SetPosition(const glm::dvec2 & pos)
{
    prev_pos_ = pos_;
    pos_ = pos;
}

bool Dog::IsStopped() const noexcept
{
    return glm::length2(velocity_) < std::numeric_limits<double>::epsilon();
}

std::string Dog::GetDirectionCode() const noexcept
{
    std::string s;
    s += (static_cast<char>(direction_));
    return s;
}

void Dog::SetDirectionCode(std::string_view d, float s)
{
    if (d.empty())
        velocity_ = { 0, 0 };
    else
    {
        switch (d[0])
        {
        case 'L':
            direction_ = Direction::West;
            velocity_  = { -s, 0 };
            break;

        case 'R':
            direction_ = Direction::East;
            velocity_  = { s, 0 };
            break;

        case 'U':
            direction_ = Direction::North;
            velocity_  = { 0, -s };
            break;

        case 'D':
            direction_ = Direction::South;
            velocity_  = { 0, s };
            break;

        default:
            velocity_  = { 0, 0 };
            break;
        }
    }
}

bool Dog::GatherItem(const LootInstancePtr & item, size_t maxBagCapacity)
{
    bool bRet = false;

    if (!item->gathered && gathered_items_.size() < maxBagCapacity) {
        item->gathered = true;
        bRet = true;

        gathered_items_.push_back(item);
    }

    return bRet;
}

void Dog::StoreLootsAtOffice(const LootTypes & lootTypes) noexcept
{
    for (const auto & loot : gathered_items_) {
        if (const auto & lt = lootTypes[loot->type]; lt.value)
            score_ += static_cast<int>(*lt.value);
    }

    gathered_items_.clear();
}

} // namespace model
