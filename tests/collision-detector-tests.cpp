//#define _USE_MATH_DEFINES

#include <catch2/catch_test_macros.hpp>
#include "../src/lib/collision_detector.h"

using namespace collision_detector;


class CItemGathererProviderMock : public ItemGathererProvider
{
public:

    CItemGathererProviderMock() = default;

    void AddItem(const Item & item) {
        items_.push_back(item);
    }

    void AddGatherer(const Gatherer & gatherer) {
        gatherers_.push_back(gatherer);
    }

    [[nodiscard]] size_t ItemsCount() const override {
        return items_.size();
    }
    [[nodiscard]] Item GetItem(size_t idx) const override {
        return items_[idx];
    }
    [[nodiscard]] size_t GatherersCount() const override {
        return gatherers_.size();
    }
    [[nodiscard]] Gatherer GetGatherer(size_t idx) const override {
        return gatherers_[idx];
    }

private:

    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};


TEST_CASE("TryCollectPoint testing")
{
    glm::dvec2 a = { 0, 0 };
    glm::dvec2 b = { 0, 3 };
    glm::dvec2 c = { 0, 2 };

    auto res = TryCollectPoint(a, b, c);

    CHECK(res.IsCollected(0.5f));
    REQUIRE(std::abs(res.proj_ratio - c.y / b.y) <= std::numeric_limits<decltype(a.x)>::epsilon());
}

TEST_CASE("FindGatherEvents test")
{
    CItemGathererProviderMock itemGathererProvider;

    auto res = FindGatherEvents(itemGathererProvider);
    REQUIRE(res.empty());

    itemGathererProvider.AddItem({.position = { 1, 1 }, .width = 0.5 } );
    itemGathererProvider.AddGatherer( {.end_pos = { 2, 2 }, .width = 0.5 } );

    res = FindGatherEvents(itemGathererProvider);
    REQUIRE(res.size() == 1);
}
