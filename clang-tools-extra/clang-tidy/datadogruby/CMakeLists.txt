set(LLVM_LINK_COMPONENTS
  FrontendOpenMP
  Support
  )

add_clang_library(clangTidyDatadogRubyModule
  DangerousInteractionsCheck.cpp
  WithoutGVLCheck.cpp
  DatadogRubyTidyModule.cpp

  LINK_LIBS
  clangTidy
  clangTidyUtils

  DEPENDS
  omp_gen
  ClangDriverOptions
  )

clang_target_link_libraries(clangTidyDatadogRubyModule
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangLex
  )
