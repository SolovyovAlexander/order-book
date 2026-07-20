# Runs EXE feeding INPUT on stdin, compares captured stdout to EXPECTED.
# Portable (pure CMake, no shell). Invoked via `cmake -P` from add_test.
execute_process(
    COMMAND "${EXE}"
    INPUT_FILE "${INPUT}"
    OUTPUT_VARIABLE actual
    RESULT_VARIABLE rc
)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "binary exited with code ${rc}")
endif()

file(READ "${EXPECTED}" expected)
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR
        "golden mismatch\n--- expected ---\n${expected}\n--- actual ---\n${actual}")
endif()
