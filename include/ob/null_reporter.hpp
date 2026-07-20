#pragma once

#include "ob/reporter.hpp"

namespace ob {

// Discards all output, so the benchmark can measure matching without formatting
// or I/O (the virtual calls still happen, keeping dispatch in the measurement).
class NullReporter : public Reporter {
public:
    void onTrade(const TradeEvent&) override {}
    void onFullyFilled(const OrderFullyFilled&) override {}
    void onPartiallyFilled(const OrderPartiallyFilled&) override {}
};

}  // namespace ob
