#include "ob/price.hpp"

#include <charconv>
#include <limits>

namespace ob {

namespace {

bool parseAllDigits(std::string_view sv, int64_t& out) {
    if (sv.empty()) {
        out = 0;
        return true;
    }
    const char* begin = sv.data();
    const char* end = sv.data() + sv.size();
    auto res = std::from_chars(begin, end, out);
    return res.ec == std::errc{} && res.ptr == end;
}

}  // namespace

std::optional<Price> Price::parse(std::string_view s) {
    if (s.empty()) return std::nullopt;

    // Reject an explicit '+'; accept a leading '-' for negative prices (real
    // markets do go negative, e.g. WTI crude on 2020-04-20). from_chars would
    // mishandle the sign, so we strip it and parse the magnitude ourselves.
    if (s.front() == '+') return std::nullopt;
    bool negative = false;
    if (s.front() == '-') {
        negative = true;
        s.remove_prefix(1);
        if (s.empty()) return std::nullopt;  // lone "-"
    }

    auto dot = s.find('.');
    std::string_view intPart = (dot == std::string_view::npos) ? s : s.substr(0, dot);
    std::string_view fracPart =
        (dot == std::string_view::npos) ? std::string_view{} : s.substr(dot + 1);

    if (dot != std::string_view::npos && (intPart.empty() || fracPart.empty())) return std::nullopt;

    constexpr std::size_t maxFrac = static_cast<std::size_t>(SCALE_DIGITS);
    if (fracPart.size() > maxFrac) return std::nullopt;

    int64_t intValue = 0;
    int64_t fracValue = 0;
    if (!parseAllDigits(intPart, intValue)) return std::nullopt;
    if (!parseAllDigits(fracPart, fracValue)) return std::nullopt;

    // 0.000000100 -> 100
    // 0.1 -> 100000000
    for (std::size_t i = fracPart.size(); i < maxFrac; ++i) fracValue *= 10;

    // Compose the magnitude in uint64 and guard overflow on both sides. A
    // negative price reaches one further than a positive one, because
    // -INT64_MIN == INT64_MAX + 1.
    constexpr uint64_t scale = static_cast<uint64_t>(SCALE);
    constexpr uint64_t maxMagPos = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    const uint64_t maxMag = negative ? maxMagPos + 1 : maxMagPos;

    const uint64_t iv = static_cast<uint64_t>(intValue);
    const uint64_t fv = static_cast<uint64_t>(fracValue);
    if (iv > (maxMag - fv) / scale) return std::nullopt;  // would exceed the bound
    const uint64_t mag = iv * scale + fv;

    int64_t scaled;
    if (negative) {
        scaled = (mag == maxMagPos + 1) ? std::numeric_limits<int64_t>::min()
                                        : -static_cast<int64_t>(mag);
    } else {
        scaled = static_cast<int64_t>(mag);
    }
    return Price(scaled);
}

std::string Price::toString() const {
    // Work on the magnitude so integer / and % never see a sign (their results
    // for negatives would complicate digit extraction). -(uint64)scaled_ yields
    // the correct magnitude for every value, INT64_MIN included.
    const bool negative = scaled_ < 0;
    const uint64_t mag =
        negative ? -static_cast<uint64_t>(scaled_) : static_cast<uint64_t>(scaled_);
    const std::string sign = negative ? "-" : "";

    constexpr uint64_t scale = static_cast<uint64_t>(SCALE);
    const uint64_t whole = mag / scale;
    uint64_t frac = mag % scale;

    if (frac == 0) return sign + std::to_string(whole);

    constexpr int kDigits = SCALE_DIGITS;
    char buf[kDigits];
    for (int i = kDigits - 1; i >= 0; --i) {
        buf[i] = static_cast<char>('0' + frac % 10);
        frac /= 10;
    }

    int len = kDigits;
    while (len > 0 && buf[len - 1] == '0') --len;

    return sign + std::to_string(whole) + '.' + std::string(buf, buf + len);
}

}  // namespace ob
