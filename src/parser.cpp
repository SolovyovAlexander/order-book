#include "ob/parser.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <optional>

#include "ob/price.hpp"
#include "ob/side.hpp"

namespace ob {

namespace {

// AddOrderRequest is the longest message: 5 fields.
constexpr std::size_t kMaxFields = 5;

std::string_view trim(std::string_view s) {
    constexpr const char* ws = " \t\r\n\v\f";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string_view::npos) return {};
    const auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

// Split `line` on ',' into `fields`. Returns the total field count; if it
// exceeds kMaxFields the surplus is not stored and the caller treats the line
// as malformed (the count alone reveals the mismatch).
std::size_t split(std::string_view line, std::array<std::string_view, kMaxFields>& fields) {
    std::size_t count = 0;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = line.find(',', start);
        const std::string_view field = (comma == std::string_view::npos)
                                           ? line.substr(start)
                                           : line.substr(start, comma - start);
        if (count < kMaxFields) fields[count] = field;
        ++count;
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    return count;
}

// Parse an unsigned integer, requiring the whole field to be consumed and no
// sign. Empty / "+5" / "-5" / "5x" all fail.
std::optional<std::uint64_t> parseUInt(std::string_view sv) {
    if (sv.empty() || sv.front() == '+' || sv.front() == '-') return std::nullopt;
    std::uint64_t value = 0;
    const auto* end = sv.data() + sv.size();
    const auto res = std::from_chars(sv.data(), end, value);
    if (res.ec != std::errc{} || res.ptr != end) return std::nullopt;
    return value;
}

std::optional<Side> parseSide(std::string_view sv) {
    if (sv == "0") return Side::Buy;
    if (sv == "1") return Side::Sell;
    return std::nullopt;
}

ParseResult error(std::string message) {
    return ParseError{std::move(message)};
}

}  // namespace

ParseResult parseLine(std::string_view raw) {
    const std::string_view line = trim(raw);
    if (line.empty()) return error("Empty input line");

    std::array<std::string_view, kMaxFields> fields{};
    const std::size_t count = split(line, fields);
    const std::string_view type = fields[0];  // count >= 1 since line is non-empty

    if (type == "0") {
        if (count != 5)
            return error("Malformed AddOrderRequest (expected 5 fields): " + std::string(line));
        const auto id = parseUInt(fields[1]);
        const auto side = parseSide(fields[2]);
        const auto quantity = parseUInt(fields[3]);
        const auto price = Price::parse(fields[4]);
        if (!id || *id == 0) return error("Invalid order id: " + std::string(fields[1]));
        if (!side) return error("Invalid side: " + std::string(fields[2]));
        if (!quantity || *quantity == 0)
            return error("Invalid quantity: " + std::string(fields[3]));
        if (!price) return error("Invalid price: " + std::string(fields[4]));
        return Request{AddOrderRequest{*id, *side, *quantity, *price}};
    }

    if (type == "1") {
        if (count != 2)
            return error("Malformed CancelOrderRequest (expected 2 fields): " + std::string(line));
        const auto id = parseUInt(fields[1]);
        if (!id || *id == 0) return error("Invalid order id: " + std::string(fields[1]));
        return Request{CancelOrderRequest{*id}};
    }

    return error("Unknown message type: " + std::string(type));
}

}  // namespace ob
