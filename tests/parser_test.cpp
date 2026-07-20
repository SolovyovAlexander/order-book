#include "ob/parser.hpp"

#include <variant>

#include "ob/price.hpp"
#include "ob/side.hpp"
#include "test_util.hpp"

using namespace ob;

static bool isError(const ParseResult& r) {
    return std::holds_alternative<ParseError>(r);
}

// Returns the AddOrderRequest if the result is a valid add, else nullptr.
static const AddOrderRequest* asAdd(const ParseResult& r) {
    if (const auto* req = std::get_if<Request>(&r)) return std::get_if<AddOrderRequest>(req);
    return nullptr;
}
static const CancelOrderRequest* asCancel(const ParseResult& r) {
    if (const auto* req = std::get_if<Request>(&r)) return std::get_if<CancelOrderRequest>(req);
    return nullptr;
}

static void test_valid_add() {
    auto r = parseLine("0,1000008,0,3,1050");
    const auto* add = asAdd(r);
    CHECK(add != nullptr);
    if (add) {
        CHECK_EQ(add->id, OrderId{1000008});
        CHECK(add->side == Side::Buy);
        CHECK_EQ(add->quantity, Quantity{3});
        CHECK(add->price == *Price::parse("1050"));
    }
}

static void test_valid_add_sell_and_decimal_price() {
    auto r = parseLine("0,7,1,10,1025.5");
    const auto* add = asAdd(r);
    CHECK(add != nullptr);
    if (add) {
        CHECK(add->side == Side::Sell);
        CHECK(add->price == *Price::parse("1025.5"));
    }
}

static void test_valid_add_negative_price() {
    // negative prices are valid (e.g. commodity/power markets)
    auto r = parseLine("0,42,1,5,-37.63");
    const auto* add = asAdd(r);
    CHECK(add != nullptr);
    if (add) CHECK(add->price == *Price::parse("-37.63"));
}

static void test_valid_cancel() {
    auto r = parseLine("1,1000004");
    const auto* cancel = asCancel(r);
    CHECK(cancel != nullptr);
    if (cancel) CHECK_EQ(cancel->id, OrderId{1000004});
}

static void test_whitespace_and_crlf() {
    CHECK(asAdd(parseLine("  0,1,0,5,100  ")) != nullptr);
    CHECK(asAdd(parseLine("0,1,0,5,100\r")) != nullptr);  // CRLF input
}

static void test_unknown_type() {
    CHECK(isError(parseLine("BADMESSAGE")));
    CHECK(isError(parseLine("2,1,2,3")));  // 2 is an output type, not input
    CHECK(isError(parseLine("")));         // empty
    CHECK(isError(parseLine("   ")));      // whitespace only
}

static void test_wrong_field_count() {
    CHECK(isError(parseLine("0,1,0,5")));        // add missing price
    CHECK(isError(parseLine("0,1,0,5,100,9")));  // add extra field
    CHECK(isError(parseLine("1")));              // cancel missing id
    CHECK(isError(parseLine("1,1,extra")));      // cancel extra field
}

static void test_bad_fields() {
    CHECK(isError(parseLine("0,0,0,5,100")));    // id 0 not positive
    CHECK(isError(parseLine("0,abc,0,5,100")));  // non-numeric id
    CHECK(isError(parseLine("0,1,2,5,100")));    // side 2 invalid
    CHECK(isError(parseLine("0,1,0,0,100")));    // quantity 0 not positive
    CHECK(isError(parseLine("0,1,0,-5,100")));   // negative quantity
    CHECK(isError(parseLine("0,1,0,5,1.2.3")));  // bad price
    CHECK(isError(parseLine("0,1,0,5,")));       // empty price
    CHECK(isError(parseLine("1,xyz")));          // cancel bad id
    CHECK(isError(parseLine("1,0")));            // cancel id 0
}

int main() {
    test_valid_add();
    test_valid_add_sell_and_decimal_price();
    test_valid_add_negative_price();
    test_valid_cancel();
    test_whitespace_and_crlf();
    test_unknown_type();
    test_wrong_field_count();
    test_bad_fields();
    return testSummary();
}
