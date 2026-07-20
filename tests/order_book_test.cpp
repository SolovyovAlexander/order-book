#include "ob/order_book.hpp"

#include <vector>

#include "collecting_reporter.hpp"
#include "ob/messages.hpp"
#include "ob/price.hpp"
#include "ob/side.hpp"
#include "test_util.hpp"

using namespace ob;
using ob::test::CollectingReporter;

static Price P(const char* s) {
    return *Price::parse(s);
}

static AddOrderRequest buy(OrderId id, Quantity qty, const char* price) {
    return AddOrderRequest{id, Side::Buy, qty, P(price)};
}
static AddOrderRequest sell(OrderId id, Quantity qty, const char* price) {
    return AddOrderRequest{id, Side::Sell, qty, P(price)};
}

// ---- Golden test: the exact example from the assignment --------------------

static void test_golden_example() {
    CollectingReporter rep;
    OrderBook book(rep);

    // Build the book (none of these cross).
    CHECK(book.add(sell(1000000, 1, "1075")));
    CHECK(book.add(buy(1000001, 9, "1000")));
    CHECK(book.add(buy(1000002, 30, "975")));
    CHECK(book.add(sell(1000003, 10, "1050")));
    CHECK(book.add(buy(1000004, 10, "950")));
    // (BADMESSAGE is a parser concern; not exercised here.)
    CHECK(book.add(sell(1000005, 2, "1025")));
    CHECK(book.add(buy(1000006, 1, "1000")));
    CHECK(book.cancel(CancelOrderRequest{1000004}));
    CHECK(book.add(sell(1000007, 5, "1025")));

    // Nothing reported until the aggressive order arrives.
    CHECK(rep.log().empty());

    // The aggressive order that triggers the trades.
    CHECK(book.add(buy(1000008, 3, "1050")));

    const std::vector<OutputMessage> expected = {
        TradeEvent{2, P("1025")},  OrderPartiallyFilled{1000008, 1},
        OrderFullyFilled{1000005}, TradeEvent{1, P("1025")},
        OrderFullyFilled{1000008}, OrderPartiallyFilled{1000007, 4},
    };
    CHECK(rep.log() == expected);
}

static void test_no_cross_just_rests() {
    CollectingReporter rep;
    OrderBook book(rep);
    book.add(buy(1, 5, "100"));
    book.add(sell(2, 5, "101"));  // spread, no cross
    CHECK(rep.log().empty());
}

static void test_exact_full_fill() {
    CollectingReporter rep;
    OrderBook book(rep);
    book.add(sell(1, 5, "100"));
    book.add(buy(2, 5, "100"));  // equal price crosses, exact quantity

    const std::vector<OutputMessage> expected = {
        TradeEvent{5, P("100")},
        OrderFullyFilled{2},  // aggressive
        OrderFullyFilled{1},  // resting
    };
    CHECK(rep.log() == expected);
}

static void test_aggressive_sweeps_multiple_levels() {
    CollectingReporter rep;
    OrderBook book(rep);
    book.add(sell(1, 2, "100"));
    book.add(sell(2, 2, "101"));
    book.add(buy(3, 5, "101"));  // eats both asks, 1 left rests as a bid

    const std::vector<OutputMessage> expected = {
        TradeEvent{2, P("100")},  // best (lowest) ask first
        OrderPartiallyFilled{3, 3}, OrderFullyFilled{1}, TradeEvent{2, P("101")},
        OrderPartiallyFilled{3, 1}, OrderFullyFilled{2},
    };
    CHECK(rep.log() == expected);
    // remainder of order 3 now rests; a matching sell should hit it
    book.add(sell(4, 1, "101"));
    CHECK(rep.log().size() == 6 + 3);  // one more trade triad
}

static void test_time_priority_within_level() {
    CollectingReporter rep;
    OrderBook book(rep);
    book.add(sell(1, 2, "100"));  // older
    book.add(sell(2, 2, "100"));  // newer, same price
    book.add(buy(3, 3, "100"));   // fills older first, then partial of newer

    const std::vector<OutputMessage> expected = {
        TradeEvent{2, P("100")},    OrderPartiallyFilled{3, 1},
        OrderFullyFilled{1},  // oldest matched first
        TradeEvent{1, P("100")},    OrderFullyFilled{3},
        OrderPartiallyFilled{2, 1},
    };
    CHECK(rep.log() == expected);
}

static void test_trade_at_resting_price_improvement() {
    CollectingReporter rep;
    OrderBook book(rep);
    book.add(buy(1, 1, "100"));  // resting buy @ 100
    book.add(sell(2, 1, "95"));  // aggressive sell willing to take >= 95

    // Trade executes at the resting price (100), not the aggressor's 95.
    const std::vector<OutputMessage> expected = {
        TradeEvent{1, P("100")},
        OrderFullyFilled{2},
        OrderFullyFilled{1},
    };
    CHECK(rep.log() == expected);
}

static void test_negative_prices_match() {
    CollectingReporter rep;
    OrderBook book(rep);
    // Resting sell at -20; aggressive buy at -10 crosses (-10 >= -20). The trade
    // executes at the resting price, -20. Matching is sign-agnostic.
    book.add(sell(1, 5, "-20"));
    book.add(buy(2, 5, "-10"));

    const std::vector<OutputMessage> expected = {
        TradeEvent{5, P("-20")},
        OrderFullyFilled{2},
        OrderFullyFilled{1},
    };
    CHECK(rep.log() == expected);
}

static void test_negative_prices_no_cross() {
    CollectingReporter rep;
    OrderBook book(rep);
    book.add(buy(1, 5, "-20"));   // pays at most -20
    book.add(sell(2, 5, "-10"));  // wants at least -10; -10 <= -20 is false -> no cross
    CHECK(rep.log().empty());
}

static void test_cancel_removes_resting() {
    CollectingReporter rep;
    OrderBook book(rep);
    book.add(sell(1, 5, "100"));
    CHECK(book.cancel(CancelOrderRequest{1}));
    book.add(buy(2, 5, "100"));  // nothing to match anymore
    CHECK(rep.log().empty());
}

static void test_cancel_unknown_returns_false() {
    CollectingReporter rep;
    OrderBook book(rep);
    CHECK(!book.cancel(CancelOrderRequest{999}));
}

static void test_duplicate_id_rejected() {
    CollectingReporter rep;
    OrderBook book(rep);
    CHECK(book.add(sell(1, 5, "100")));
    CHECK(!book.add(sell(1, 3, "101")));  // same live id -> rejected
    // first order still intact: a buy fills exactly 5
    book.add(buy(2, 5, "100"));
    CHECK_EQ(rep.log().size(), std::size_t{3});
}

int main() {
    test_golden_example();
    test_no_cross_just_rests();
    test_exact_full_fill();
    test_aggressive_sweeps_multiple_levels();
    test_time_priority_within_level();
    test_trade_at_resting_price_improvement();
    test_negative_prices_match();
    test_negative_prices_no_cross();
    test_cancel_removes_resting();
    test_cancel_unknown_returns_false();
    test_duplicate_id_rejected();
    return testSummary();
}
