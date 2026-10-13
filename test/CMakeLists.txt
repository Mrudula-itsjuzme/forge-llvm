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
