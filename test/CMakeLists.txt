find_program(FORGE_OPT NAMES opt HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
find_program(FORGE_FILECHECK NAMES FileCheck HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(GLOB FORGE_IR_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ir/*.ll")
foreach(TEST_FILE IN LISTS FORGE_IR_TESTS)
  get_filename_component(TEST_NAME "${TEST_FILE}" NAME_WE)
  add_test(NAME "ir.${TEST_NAME}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_filecheck.py"
      --opt "${FORGE_OPT}"
      --filecheck "${FORGE_FILECHECK}"
      --plugin "$<TARGET_FILE:ForgePass>"
      --test "${TEST_FILE}")
endforeach()

add_test(NAME randomized.verifier
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/randomized_ir.py"
    --opt "${FORGE_OPT}" --plugin "$<TARGET_FILE:ForgePass>" --seed 20261013)

