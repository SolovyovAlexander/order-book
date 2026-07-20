# Generates a scenario with GEN and feeds it to ENGINE, asserting the engine
# never exits non-zero (no crash; under OB_SANITIZE, ASan/UBSan would also abort
# here on any memory error or UB). Invoked via `cmake -P` from add_test.
execute_process(
    COMMAND "${GEN}" "${SCENARIO}" "${COUNT}" "${SEED}"
    OUTPUT_FILE "${TMP}"
    RESULT_VARIABLE gen_rc
)
if(NOT gen_rc EQUAL 0)
    message(FATAL_ERROR "generator failed (${gen_rc})")
endif()

execute_process(
    COMMAND "${ENGINE}"
    INPUT_FILE "${TMP}"
    OUTPUT_QUIET
    RESULT_VARIABLE rc
)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "engine exited non-zero (${rc}) on '${SCENARIO}' input")
endif()
