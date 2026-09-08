add_llvm_library(aliasCustom MODULE BUILDTREE_ONLY
  alias_lib.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )
