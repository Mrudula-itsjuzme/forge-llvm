find_program(FORGE_OPT NAMES opt HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
find_program(FORGE_FILECHECK NAMES FileCheck HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(GLOB_RECURSE FORGE_IR_TESTS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/ir/*.ll"
  "${CMAKE_CURRENT_SOURCE_DIR}/global/*.ll"
  "${CMAKE_CURRENT_SOURCE_DIR}/licm/*.ll")
foreach(TEST_FILE IN LISTS FORGE_IR_TESTS)
  get_filename_component(TEST_NAME "${TEST_FILE}" NAME_WE)
  if(TEST_FILE MATCHES "/global/")
    set(FORGE_TEST_PIPELINE "forge-global-cse")
    set(FORGE_TEST_GROUP "global")
  elseif(TEST_FILE MATCHES "/licm/")
    set(FORGE_TEST_PIPELINE "forge-licm")
    set(FORGE_TEST_GROUP "licm")
  else()
    set(FORGE_TEST_PIPELINE "forge-local-cse")
    set(FORGE_TEST_GROUP "local")
  endif()
  add_test(NAME "${FORGE_TEST_GROUP}.${TEST_NAME}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_filecheck.py"
      --opt "${FORGE_OPT}"
      --filecheck "${FORGE_FILECHECK}"
      --plugin "$<TARGET_FILE:ForgePass>"
      --passes "${FORGE_TEST_PIPELINE}"
      --test "${TEST_FILE}")
endforeach()

add_test(NAME randomized.verifier
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/randomized_ir.py"
    --opt "${FORGE_OPT}" --plugin "$<TARGET_FILE:ForgePass>" --seed 20261013)

find_program(FORGE_CLANG NAMES clang clang-18 HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
add_test(NAME differential.execution
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/differential/run_differential.py"
    --clang "${FORGE_CLANG}" --opt "${FORGE_OPT}"
    --plugin "$<TARGET_FILE:ForgePass>" --seed 20261013)

add_test(NAME differential.sanitized
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/differential/run_differential.py"
    --clang "${FORGE_CLANG}" --opt "${FORGE_OPT}"
    --plugin "$<TARGET_FILE:ForgePass>" --seed 20261013 --sanitize
    --allow-sanitizer-unavailable)
set_tests_properties(differential.sanitized PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME differential.mutation-stress
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/differential/mutation_stress.py"
    --clang "${FORGE_CLANG}" --opt "${FORGE_OPT}"
    --plugin "$<TARGET_FILE:ForgePass>" --seed 20261013 --cases 250
    --failure-dir "${CMAKE_CURRENT_BINARY_DIR}/mutation-failures")

add_test(NAME diagnostics.structured
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/run_diagnostics.py"
    --opt "${FORGE_OPT}" --plugin "$<TARGET_FILE:ForgePass>"
    --input "${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/cases.ll")

add_test(NAME experiments.registry-smoke
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/experiments/run_registry_smoke.py"
    --python "${Python3_EXECUTABLE}" --clang "${FORGE_CLANG}"
    --opt "${FORGE_OPT}" --plugin "$<TARGET_FILE:ForgePass>"
    --source-root "${PROJECT_SOURCE_DIR}")

find_program(FORGE_LLVM_MCA NAMES llvm-mca HINTS "${LLVM_TOOLS_BINARY_DIR}")
if(FORGE_LLVM_MCA)
  add_test(NAME backend.static-analysis
    COMMAND "${Python3_EXECUTABLE}"
      "${PROJECT_SOURCE_DIR}/analysis/analyze_backends.py"
      --clang "${FORGE_CLANG}" --llvm-mca "${FORGE_LLVM_MCA}"
      --commit ctest --output "${CMAKE_CURRENT_BINARY_DIR}/backend-smoke")
endif()

find_program(FORGE_ALIVE_TV NAMES alive-tv)
if(FORGE_ALIVE_TV)
  add_test(NAME formal.alive2
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/formal/run_alive2.py"
      --alive-tv "${FORGE_ALIVE_TV}" --opt "${FORGE_OPT}"
      --plugin "$<TARGET_FILE:ForgePass>"
      --input "${CMAKE_CURRENT_SOURCE_DIR}/formal/local-cse.ll"
      --status "${CMAKE_CURRENT_BINARY_DIR}/alive2-status.json")
else()
  add_test(NAME formal.alive2-unavailable
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/formal/run_alive2.py"
      --alive-tv alive-tv --opt "${FORGE_OPT}"
      --plugin "$<TARGET_FILE:ForgePass>"
      --input "${CMAKE_CURRENT_SOURCE_DIR}/formal/local-cse.ll"
      --status "${CMAKE_CURRENT_BINARY_DIR}/alive2-status.json")
  set_tests_properties(formal.alive2-unavailable PROPERTIES SKIP_RETURN_CODE 77)
endif()
