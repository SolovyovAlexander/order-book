#pragma once

#include <vector>

#include "ob/messages.hpp"
#include "ob/reporter.hpp"

namespace ob::test {

// Test double: records every reported message, in order, so a test can assert
// the exact output sequence the engine produced. Reused by the engine's golden
// test in Part 5.
class CollectingReporter : public Reporter {
public:
    void onTrade(const TradeEvent& t) override { log_.emplace_back(t); }
    void onFullyFilled(const OrderFullyFilled& m) override { log_.emplace_back(m); }
    void onPartiallyFilled(const OrderPartiallyFilled& m) override { log_.emplace_back(m); }

    const std::vector<OutputMessage>& log() const { return log_; }

private:
    std::vector<OutputMessage> log_;
};

}  // namespace ob::test
