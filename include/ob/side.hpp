#pragma once

#include <cstdint>

namespace ob {

// Values match the protocol wire format (0=Buy, 1=Sell), so the parser maps the
// input field straight onto the enum.
enum class Side : std::uint8_t {
    Buy = 0,
    Sell = 1,
};

}  // namespace ob
