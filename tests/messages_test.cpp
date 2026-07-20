#include "ob/messages.hpp"

#include <cstdint>
#include <variant>

#include "ob/order.hpp"
#include "ob/side.hpp"
#include "test_util.hpp"

using namespace ob;

static void test_side_wire_values() {
    // The numeric values must match the protocol; the parser relies on them.
    CHECK_EQ(static_cast<std::uint8_t>(Side::Buy), std::uint8_t{0});
    CHECK_EQ(static_cast<std::uint8_t>(Side::Sell), std::uint8_t{1});
}

static void test_order_fields() {
    // Order is the lean resting node: just id + remaining quantity.
    Order o{42, 7};
    CHECK_EQ(o.id, OrderId{42});
    CHECK_EQ(o.quantity, Quantity{7});

    // default-constructed holder is well-defined
    Order d{};
    CHECK_EQ(d.id, OrderId{0});
    CHECK_EQ(d.quantity, Quantity{0});
}

static void test_request_variant() {
    Request add = AddOrderRequest{1, Side::Buy, 9, *Price::parse("1000")};
    CHECK(std::holds_alternative<AddOrderRequest>(add));
    CHECK_EQ(std::get<AddOrderRequest>(add).quantity, Quantity{9});

    Request cancel = CancelOrderRequest{1000004};
    CHECK(std::holds_alternative<CancelOrderRequest>(cancel));
    CHECK_EQ(std::get<CancelOrderRequest>(cancel).id, OrderId{1000004});
}

static void test_output_message_fields() {
    TradeEvent t{2, *Price::parse("1025")};
    CHECK_EQ(t.quantity, Quantity{2});
    CHECK(t.price == *Price::parse("1025"));

    OrderFullyFilled f{1000005};
    CHECK_EQ(f.id, OrderId{1000005});

    OrderPartiallyFilled p{1000007, 4};
    CHECK_EQ(p.id, OrderId{1000007});
    CHECK_EQ(p.quantity, Quantity{4});
}

int main() {
    test_side_wire_values();
    test_order_fields();
    test_request_variant();
    test_output_message_fields();
    return testSummary();
}
