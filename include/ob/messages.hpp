#pragma once

#include <variant>

#include "ob/order.hpp"
#include "ob/price.hpp"
#include "ob/side.hpp"

namespace ob {

// Input requests (parsed from stdin).

// 0,orderid,side,quantity,price
struct AddOrderRequest {
    OrderId id = 0;
    Side side = Side::Buy;
    Quantity quantity = 0;
    Price price;
};

// 1,orderid
struct CancelOrderRequest {
    OrderId id = 0;
};

using Request = std::variant<AddOrderRequest, CancelOrderRequest>;

// Output messages, emitted through the Reporter.

// 2,quantity,price
struct TradeEvent {
    Quantity quantity = 0;
    Price price;

    bool operator==(const TradeEvent& o) const {
        return quantity == o.quantity && price == o.price;
    }
};

// 3,orderid
struct OrderFullyFilled {
    OrderId id = 0;

    bool operator==(const OrderFullyFilled& o) const { return id == o.id; }
};

// 4,orderid,quantity  (new remaining quantity)
struct OrderPartiallyFilled {
    OrderId id = 0;
    Quantity quantity = 0;

    bool operator==(const OrderPartiallyFilled& o) const {
        return id == o.id && quantity == o.quantity;
    }
};

// Collected by the test reporter to compare a whole output sequence; variant
// gives operator== once each alternative has it.
using OutputMessage = std::variant<TradeEvent, OrderFullyFilled, OrderPartiallyFilled>;

}  // namespace ob
