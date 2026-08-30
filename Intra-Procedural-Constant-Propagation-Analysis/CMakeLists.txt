add_llvm_library(cpCustom MODULE BUILDTREE_ONLY
  cp_lib.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )
