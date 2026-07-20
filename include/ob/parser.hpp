#pragma once

#include <string>
#include <string_view>
#include <variant>

#include "ob/messages.hpp"

namespace ob {

// Why a line could not be parsed; the caller logs `message` to stderr.
struct ParseError {
    std::string message;
};

// Either a valid request or a parse error. The parser does no I/O - keeping it
// pure makes it trivially testable and leaves stderr to main.
using ParseResult = std::variant<Request, ParseError>;

// Parse a single input line into a request. Surrounding whitespace (incl. a
// trailing CR from CRLF input) is ignored; fields themselves must be clean.
ParseResult parseLine(std::string_view line);

}  // namespace ob
