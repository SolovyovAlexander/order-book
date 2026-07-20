cmake_minimum_required(VERSION 3.16)
project(order_book CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

option(OB_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
if(OB_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(ob_core
    src/price.cpp
    src/order_book.cpp
    src/parser.cpp
    src/stdout_reporter.cpp
)
target_include_directories(ob_core PUBLIC include)

add_executable(order_book src/main.cpp)
target_link_libraries(order_book PRIVATE ob_core)

add_executable(gen tools/gen.cpp)

add_executable(bench tools/bench.cpp)
target_link_libraries(bench PRIVATE ob_core)

# Code formatting: `cmake --build build --target format` (and format-check).
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE OB_FORMAT_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp)
    add_custom_target(format COMMAND ${CLANG_FORMAT} -i ${OB_FORMAT_SOURCES})
    add_custom_target(format-check COMMAND ${CLANG_FORMAT} --dry-run --Werror ${OB_FORMAT_SOURCES})
endif()

enable_testing()

add_executable(price_test tests/price_test.cpp)
target_link_libraries(price_test PRIVATE ob_core)
target_include_directories(price_test PRIVATE tests)
add_test(NAME price_test COMMAND price_test)

add_executable(messages_test tests/messages_test.cpp)
target_link_libraries(messages_test PRIVATE ob_core)
target_include_directories(messages_test PRIVATE tests)
add_test(NAME messages_test COMMAND messages_test)

add_executable(level_test tests/level_test.cpp)
target_link_libraries(level_test PRIVATE ob_core)
target_include_directories(level_test PRIVATE tests)
add_test(NAME level_test COMMAND level_test)

add_executable(reporter_test tests/reporter_test.cpp)
target_link_libraries(reporter_test PRIVATE ob_core)
target_include_directories(reporter_test PRIVATE tests)
add_test(NAME reporter_test COMMAND reporter_test)

add_executable(order_book_test tests/order_book_test.cpp)
target_link_libraries(order_book_test PRIVATE ob_core)
target_include_directories(order_book_test PRIVATE tests)
add_test(NAME order_book_test COMMAND order_book_test)

add_executable(parser_test tests/parser_test.cpp)
target_link_libraries(parser_test PRIVATE ob_core)
target_include_directories(parser_test PRIVATE tests)
add_test(NAME parser_test COMMAND parser_test)

add_executable(stdout_reporter_test tests/stdout_reporter_test.cpp)
target_link_libraries(stdout_reporter_test PRIVATE ob_core)
target_include_directories(stdout_reporter_test PRIVATE tests)
add_test(NAME stdout_reporter_test COMMAND stdout_reporter_test)

# End-to-end golden test: feed the assignment example through the real binary and
# diff stdout against the expected output.
add_test(NAME golden_io COMMAND ${CMAKE_COMMAND}
    -DEXE=$<TARGET_FILE:order_book>
    -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/example.in
    -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/example.out
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_golden.cmake)

# No-crash stress tests: large generated inputs (incl. ~30% malformed) through the
# real binary. Most valuable under -DOB_SANITIZE=ON, which turns any memory error
# or UB into a non-zero exit and fails the test.
foreach(scenario fuzz mixed cross cancel)
    add_test(NAME stress_${scenario} COMMAND ${CMAKE_COMMAND}
        -DGEN=$<TARGET_FILE:gen>
        -DENGINE=$<TARGET_FILE:order_book>
        -DSCENARIO=${scenario}
        -DCOUNT=200000
        -DSEED=1
        -DTMP=${CMAKE_CURRENT_BINARY_DIR}/stress_${scenario}.in
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_stress.cmake)
endforeach()
