add_executable(forge-analyze
  main.cpp
  "${PROJECT_SOURCE_DIR}/lib/Expression.cpp"
)
target_include_directories(forge-analyze PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(forge-analyze PRIVATE cxx_std_17)
target_compile_options(forge-analyze PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(forge-analyze PRIVATE LLVM)
