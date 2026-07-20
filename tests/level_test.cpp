#include "ob/level.hpp"

#include "ob/order.hpp"
#include "ob/pool.hpp"
#include "ob/price.hpp"
#include "test_util.hpp"

using namespace ob;

static Order mk(OrderId id, Quantity qty) {
    return Order{id, qty};
}

static void test_empty() {
    Pool pool;
    Level lvl(pool);
    CHECK(lvl.empty());
    CHECK_EQ(lvl.size(), std::size_t{0});
}

static void test_fifo_order() {
    Pool pool;
    Level lvl(pool);
    lvl.add(mk(1, 2));
    lvl.add(mk(2, 5));
    lvl.add(mk(3, 7));

    CHECK_EQ(lvl.size(), std::size_t{3});
    // oldest first
    CHECK_EQ(lvl.front().id, OrderId{1});
    lvl.popFront();
    CHECK_EQ(lvl.front().id, OrderId{2});
    lvl.popFront();
    CHECK_EQ(lvl.front().id, OrderId{3});
    lvl.popFront();
    CHECK(lvl.empty());
}

static void test_modify_front_in_place() {
    // partial fill of the resting order at the front
    Pool pool;
    Level lvl(pool);
    lvl.add(mk(1, 5));
    lvl.front().quantity -= 1;
    CHECK_EQ(lvl.front().quantity, Quantity{4});
}

static void test_remove_by_handle_keeps_fifo() {
    Pool pool;
    Level lvl(pool);
    auto h1 = lvl.add(mk(1, 2));
    auto h2 = lvl.add(mk(2, 5));
    auto h3 = lvl.add(mk(3, 7));
    (void)h1;  // added but never referenced again; silence -Wunused-variable

    // cancel the middle order; handles to neighbours stay valid
    lvl.remove(h2);
    CHECK_EQ(lvl.size(), std::size_t{2});
    CHECK_EQ(lvl.front().id, OrderId{1});
    CHECK_EQ(lvl.at(h3).id, OrderId{3});
    lvl.popFront();
    CHECK_EQ(lvl.front().id, OrderId{3});
}

int main() {
    test_empty();
    test_fifo_order();
    test_modify_front_in_place();
    test_remove_by_handle_keeps_fifo();
    return testSummary();
}
