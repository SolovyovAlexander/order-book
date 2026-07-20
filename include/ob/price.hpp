#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ob {

namespace detail {
// compile time pow10 for int
constexpr int64_t pow10(int n) {
    int64_t result = 1;
    for (int i = 0; i < n; ++i) result *= 10;
    return result;
}
}  // namespace detail

class Price {
public:
    static constexpr int SCALE_DIGITS = 9;
    static constexpr int64_t SCALE = detail::pow10(SCALE_DIGITS);

    constexpr Price() = default;

    static std::optional<Price> parse(std::string_view text);

    std::string toString() const;

    constexpr int64_t raw() const { return scaled_; }
    static constexpr Price fromRaw(int64_t scaled) { return Price(scaled); }

    constexpr bool operator==(const Price& o) const { return scaled_ == o.scaled_; }
    constexpr bool operator!=(const Price& o) const { return scaled_ != o.scaled_; }
    constexpr bool operator<(const Price& o) const { return scaled_ < o.scaled_; }
    constexpr bool operator<=(const Price& o) const { return scaled_ <= o.scaled_; }
    constexpr bool operator>(const Price& o) const { return scaled_ > o.scaled_; }
    constexpr bool operator>=(const Price& o) const { return scaled_ >= o.scaled_; }

private:
    explicit constexpr Price(int64_t scaled) : scaled_(scaled) {}

    int64_t scaled_ = 0;
};

}  // namespace ob