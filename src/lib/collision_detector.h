#pragma once

#include "glm_include.h"

#include <algorithm>
#include <vector>

namespace collision_detector
{

struct CollectionResult
{
    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
    }

    // Квадрат расстояния до точки
    double sq_distance = 0;
    // Доля пройденного отрезка
    double proj_ratio = 0;
};

// Движемся из точки a в точку b и пытаемся подобрать точку c
CollectionResult TryCollectPoint(const glm::dvec2 & a, const glm::dvec2 & b, const glm::dvec2 & c);

struct Item
{
    glm::dvec2 position;
    double width = 0;
};

struct Gatherer
{
    glm::dvec2 start_pos;
    glm::dvec2 end_pos;
    double width = 0;
};

class ItemGathererProvider
{
protected:
    virtual ~ItemGathererProvider() = default;

public:
    [[nodiscard]] virtual size_t ItemsCount() const = 0;
    [[nodiscard]] virtual Item GetItem(size_t idx) const = 0;
    [[nodiscard]] virtual size_t GatherersCount() const = 0;
    [[nodiscard]] virtual Gatherer GetGatherer(size_t idx) const = 0;
};

struct GatheringEvent
{
    size_t item_id = 0;
    size_t gatherer_id = 0;
    double sq_distance = 0;
    double time = 0;
};

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);

}  // namespace collision_detector
