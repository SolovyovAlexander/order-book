#include <iostream>
#include <string>
#include <string_view>
#include <variant>

#include "ob/order_book.hpp"
#include "ob/parser.hpp"
#include "ob/stdout_reporter.hpp"

namespace {

bool isBlank(std::string_view line) {
    return line.find_first_not_of(" \t\r\n\v\f") == std::string_view::npos;
}

}  // namespace

int main() {
    // Detach from C stdio and don't flush per line (flushed once at exit) - a big
    // throughput win for line-oriented I/O.
    std::ios::sync_with_stdio(false);

    ob::StdoutReporter reporter(std::cout);
    ob::OrderBook book(reporter);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (isBlank(line)) continue;  // ignore blank lines (e.g. trailing newline)

        ob::ParseResult result = ob::parseLine(line);
        if (const auto* error = std::get_if<ob::ParseError>(&result)) {
            std::cerr << error->message << '\n';
            continue;
        }

        // Duplicate-id adds and unknown-id cancels return false; ignored on purpose.
        const ob::Request& request = std::get<ob::Request>(result);
        if (const auto* add = std::get_if<ob::AddOrderRequest>(&request)) {
            book.add(*add);
        } else {
            book.cancel(std::get<ob::CancelOrderRequest>(request));
        }
    }

    return 0;
}
