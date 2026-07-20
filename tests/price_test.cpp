#include "ob/price.hpp"

#include <string>

#include "test_util.hpp"

using ob::Price;

static void test_parse_integer() {
    auto p = Price::parse("1025");
    CHECK(p.has_value());
    CHECK_EQ(p->toString(), std::string("1025"));
}

static void test_parse_decimal() {
    auto p = Price::parse("1025.5");
    CHECK(p.has_value());
    CHECK_EQ(p->toString(), std::string("1025.5"));

    auto q = Price::parse("1025.50");
    CHECK(q.has_value());
    CHECK_EQ(q->toString(), std::string("1025.5"));

    auto r = Price::parse("0.000000001");
    CHECK(r.has_value());
    CHECK_EQ(r->toString(), std::string("0.000000001"));
}

static void test_equality_across_formats() {
    auto a = Price::parse("100.1");
    auto b = Price::parse("100.10");
    auto c = Price::parse("100.100000000");
    CHECK(a && b && c);
    CHECK(*a == *b);
    CHECK(*b == *c);
}

static void test_compare() {
    auto lo = Price::parse("1025");
    auto hi = Price::parse("1050");
    CHECK(lo && hi);
    CHECK(*lo < *hi);
    CHECK(*hi > *lo);
    CHECK(*lo != *hi);
    CHECK(*lo <= *lo);

    // explicit coverage for == (true case), >= (both branches), <= (strict branch)
    auto same = Price::parse("1025");
    CHECK(same && *lo == *same);
    CHECK(*lo >= *same);  // equal
    CHECK(*hi >= *lo);    // greater
    CHECK(*lo <= *hi);    // strictly less
}

static void test_dot_requires_both_sides() {
    // a lone dot, or a dot with digits on only one side, is invalid
    CHECK(!Price::parse("1025.").has_value());
    CHECK(!Price::parse(".5").has_value());
    CHECK(!Price::parse(".").has_value());
}

static void test_tostring_roundtrip_extremes() {
    // exercises the full toString buffer (all SCALE_DIGITS used) at int64 max
    auto max = Price::parse("9223372036.854775807");
    CHECK(max.has_value());
    CHECK_EQ(max->toString(), std::string("9223372036.854775807"));

    auto zero = Price::parse("0");
    CHECK(zero.has_value());
    CHECK_EQ(zero->toString(), std::string("0"));
}

static void test_negative_prices() {
    // real markets go negative (e.g. WTI crude 2020-04-20)
    auto a = Price::parse("-37.63");
    CHECK(a.has_value());
    CHECK_EQ(a->toString(), std::string("-37.63"));

    auto b = Price::parse("-0.000000001");
    CHECK(b.has_value());
    CHECK_EQ(b->toString(), std::string("-0.000000001"));

    auto c = Price::parse("-1025");
    CHECK(c.has_value());
    CHECK_EQ(c->toString(), std::string("-1025"));

    // ordering across zero
    auto neg = Price::parse("-5");
    auto zero = Price::parse("0");
    auto pos = Price::parse("5");
    CHECK(neg && zero && pos);
    CHECK(*neg < *zero);
    CHECK(*zero < *pos);

    // "-0" normalises to "0" (no negative zero printed)
    auto nz = Price::parse("-0");
    CHECK(nz.has_value());
    CHECK_EQ(nz->toString(), std::string("0"));

    // a lone '-' is invalid
    CHECK(!Price::parse("-").has_value());
    CHECK(!Price::parse("-.").has_value());
}

static void test_negative_overflow_bounds() {
    // INT64_MIN scaled = -9223372036.854775808 is the most-negative valid price
    auto lo = Price::parse("-9223372036.854775808");
    CHECK(lo.has_value());
    CHECK_EQ(lo->toString(), std::string("-9223372036.854775808"));

    // one tick beyond overflows
    CHECK(!Price::parse("-9223372036.854775809").has_value());
}

static void test_invalid() {
    CHECK(!Price::parse("").has_value());
    CHECK(!Price::parse("abc").has_value());
    CHECK(!Price::parse("10.5.5").has_value());
    CHECK(!Price::parse("+5").has_value());   // explicit plus rejected
    CHECK(!Price::parse("--5").has_value());  // double sign
    CHECK(!Price::parse("1,000").has_value());
    CHECK(!Price::parse("12x").has_value());
    CHECK(!Price::parse("1.1234567890").has_value());  // 10 digits > 9
    CHECK(!Price::parse(".").has_value());
}

static void test_overflow() {
    CHECK(Price::parse("9223372036").has_value());

    // Maximum possible for SCALE_DIGITS = 9
    CHECK(Price::parse("9223372036.854775807").has_value());

    CHECK(!Price::parse("9223372036.854775808").has_value());

    CHECK(!Price::parse("9223372037").has_value());

    CHECK(!Price::parse("99999999999999999999").has_value());

    CHECK(!Price::parse("9223372037.5").has_value());
}

int main() {
    test_parse_integer();
    test_parse_decimal();
    test_equality_across_formats();
    test_compare();
    test_dot_requires_both_sides();
    test_tostring_roundtrip_extremes();
    test_negative_prices();
    test_negative_overflow_bounds();
    test_invalid();
    test_overflow();
    return testSummary();
}
