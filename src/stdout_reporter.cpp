#include "ob/stdout_reporter.hpp"

namespace ob {

// '\n' (not std::endl) avoids a flush per line; the stream is flushed once when
// it is destroyed / the program exits.
void StdoutReporter::onTrade(const TradeEvent& trade) {
    out_ << "2," << trade.quantity << ',' << trade.price.toString() << '\n';
}

void StdoutReporter::onFullyFilled(const OrderFullyFilled& msg) {
    out_ << "3," << msg.id << '\n';
}

void StdoutReporter::onPartiallyFilled(const OrderPartiallyFilled& msg) {
    out_ << "4," << msg.id << ',' << msg.quantity << '\n';
}

}  // namespace ob
