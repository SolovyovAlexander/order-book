#include "ob/reporter.hpp"

#include "collecting_reporter.hpp"
#include "ob/messages.hpp"
#include "ob/price.hpp"
#include "test_util.hpp"

using namespace ob;
using ob::test::CollectingReporter;

// Drives a Reporter through the base interface, mirroring the per-match output
// sequence from the assignment example: Trade, aggressive fill, resting fill.
static void emitOneMatch(Reporter& r) {
    r.onTrade(TradeEvent{2, *Price::parse("1025")});
    r.onPartiallyFilled(OrderPartiallyFilled{1000008, 1});
    r.onFullyFilled(OrderFullyFilled{1000005});
}

static void test_collects_in_order() {
    CollectingReporter rep;
    emitOneMatch(rep);  // passed as Reporter& - dispatched virtually

    const auto& log = rep.log();
    CHECK_EQ(log.size(), std::size_t{3});

    const std::vector<OutputMessage> expected = {
        TradeEvent{2, *Price::parse("1025")},
        OrderPartiallyFilled{1000008, 1},
        OrderFullyFilled{1000005},
    };
    CHECK(log == expected);  // variant operator== compares type + value, in order
}

static void test_empty_log() {
    CollectingReporter rep;
    CHECK(rep.log().empty());
}

int main() {
    test_collects_in_order();
    test_empty_log();
    return testSummary();
}
