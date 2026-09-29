#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "street/core/event_bus.hpp"
#include "street/core/stats.hpp"
#include "street/core/stopwatch.hpp"

namespace stats = street::stats;

TEST_CASE("descriptive statistics") {
    const std::vector<double> v = {2, 4, 4, 4, 5, 5, 7, 9};
    CHECK(stats::mean(v) == doctest::Approx(5.0));
    CHECK(stats::stddev(v) == doctest::Approx(2.138).epsilon(0.001));
    CHECK(stats::median(v) == doctest::Approx(4.5));
    CHECK(stats::percentile(v, 0) == doctest::Approx(2.0));
    CHECK(stats::percentile(v, 100) == doctest::Approx(9.0));
    CHECK(stats::min(v) == 2.0);
    CHECK(stats::max(v) == 9.0);
    CHECK(stats::mean({}) == 0.0);
    CHECK(stats::median({}) == 0.0);
}

TEST_CASE("pearson correlation") {
    const std::vector<double> x = {1, 2, 3, 4, 5};
    const std::vector<double> up = {2, 4, 6, 8, 10};
    const std::vector<double> down = {5, 4, 3, 2, 1};
    const std::vector<double> flat = {3, 3, 3, 3, 3};
    CHECK(stats::pearson(x, up) == doctest::Approx(1.0));
    CHECK(stats::pearson(x, down) == doctest::Approx(-1.0));
    CHECK(stats::pearson(x, flat) == 0.0);
}

TEST_CASE("clamp, lerp and logistic") {
    static_assert(stats::clamp(5.0, 0.0, 1.0) == 1.0);
    static_assert(stats::lerp(0.0, 10.0, 0.25) == 2.5);
    CHECK(stats::logistic(0.0) == doctest::Approx(0.5));
    CHECK(stats::logistic(10.0) > 0.99);
}

namespace {
struct Buzzer {
    int points;
};
struct Drop {
    std::string sku;
};
} // namespace

TEST_CASE("event bus routes events by type") {
    street::EventBus bus;
    int total = 0;
    std::vector<std::string> drops;
    const auto id = bus.subscribe<Buzzer>([&](const Buzzer& b) { total += b.points; });
    bus.subscribe<Buzzer>([&](const Buzzer& b) { total += b.points * 10; });
    bus.subscribe<Drop>([&](const Drop& d) { drops.push_back(d.sku); });

    CHECK(bus.publish(Buzzer{3}) == 2);
    CHECK(total == 33);
    CHECK(bus.publish(Drop{"AJ1-CHI"}) == 1);
    CHECK(drops == std::vector<std::string>{"AJ1-CHI"});

    CHECK(bus.unsubscribe(id));
    CHECK_FALSE(bus.unsubscribe(id));
    CHECK(bus.subscriber_count<Buzzer>() == 1);
    bus.publish(Buzzer{1});
    CHECK(total == 43);
    CHECK(bus.publish(42) == 0); // nobody listens to ints
}

TEST_CASE("stopwatch is monotonic") {
    street::Stopwatch sw;
    const double a = sw.seconds();
    const double b = sw.seconds();
    CHECK(a >= 0.0);
    CHECK(b >= a);
}
