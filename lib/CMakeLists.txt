add_llvm_pass_plugin(ForgePass
  ForgePass.cpp
  PLUGIN_TOOL opt
)

target_compile_features(ForgePass PRIVATE cxx_std_17)
target_compile_options(ForgePass PRIVATE -Wall -Wextra -Wpedantic)

