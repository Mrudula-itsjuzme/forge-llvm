add_llvm_pass_plugin(ForgePass
  Expression.cpp
  GlobalCSE.cpp
  LocalCSE.cpp
  LoopInvariantCodeMotion.cpp
  Plugin.cpp
  PLUGIN_TOOL opt
)

target_include_directories(ForgePass PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(ForgePass PRIVATE cxx_std_17)
target_compile_options(ForgePass PRIVATE -Wall -Wextra -Wpedantic)
