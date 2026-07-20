#include "ob/stdout_reporter.hpp"

#include <sstream>
#include <string>

#include "ob/messages.hpp"
#include "ob/price.hpp"
#include "test_util.hpp"

using namespace ob;

static void test_format_each_type() {
    std::ostringstream os;
    StdoutReporter r(os);
    r.onTrade(TradeEvent{2, *Price::parse("1025")});
    r.onFullyFilled(OrderFullyFilled{1000005});
    r.onPartiallyFilled(OrderPartiallyFilled{1000007, 4});
    CHECK_EQ(os.str(), std::string("2,2,1025\n3,1000005\n4,1000007,4\n"));
}

static void test_format_decimal_and_negative_price() {
    std::ostringstream os;
    StdoutReporter r(os);
    r.onTrade(TradeEvent{5, *Price::parse("1025.5")});
    r.onTrade(TradeEvent{1, *Price::parse("-37.63")});
    CHECK_EQ(os.str(), std::string("2,5,1025.5\n2,1,-37.63\n"));
}

int main() {
    test_format_each_type();
    test_format_decimal_and_negative_price();
    return testSummary();
}
