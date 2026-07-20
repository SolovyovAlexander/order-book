#pragma once

#include "ob/messages.hpp"

namespace ob {

// Output sink for the engine. Matching reports every event through this
// interface and never writes to stdout itself, which keeps the engine free of
// I/O and lets tests substitute a double that captures the message stream. The
// only virtual dispatch on the hot path - negligible next to formatting and I/O.
class Reporter {
public:
    virtual ~Reporter() = default;

    virtual void onTrade(const TradeEvent& trade) = 0;
    virtual void onFullyFilled(const OrderFullyFilled& msg) = 0;
    virtual void onPartiallyFilled(const OrderPartiallyFilled& msg) = 0;
};

}  // namespace ob
