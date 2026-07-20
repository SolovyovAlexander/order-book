#pragma once

#include <ostream>

#include "ob/messages.hpp"
#include "ob/reporter.hpp"

namespace ob {

// Formats messages as protocol lines into an injected ostream (std::cout in
// production; an ostringstream in tests, to assert the exact bytes).
class StdoutReporter : public Reporter {
public:
    explicit StdoutReporter(std::ostream& out) : out_(out) {}

    void onTrade(const TradeEvent& trade) override;
    void onFullyFilled(const OrderFullyFilled& msg) override;
    void onPartiallyFilled(const OrderPartiallyFilled& msg) override;

private:
    std::ostream& out_;
};

}  // namespace ob
