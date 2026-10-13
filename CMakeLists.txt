cmake_minimum_required(VERSION 3.20)
project(Forge VERSION 0.1.0 LANGUAGES CXX)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_subdirectory(lib)
add_subdirectory(tools/forge-analyze)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
