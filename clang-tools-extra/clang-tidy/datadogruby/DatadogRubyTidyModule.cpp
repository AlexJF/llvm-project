//===--- LinuxKernelTidyModule.cpp - clang-tidy----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
// #include "DangerousInteractionsCheck.h"
#include "WithoutGVLCheck.h"

namespace clang::tidy {
namespace datadogruby {

/// This module is for checks specific to Datadog's ddtrace-rb
class DatadogRubyModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    /*CheckFactories.registerCheck<DangerousInteractionsCheck>(
        "datadogruby-dangerous-interactions");*/
    CheckFactories.registerCheck<WithoutGVLCheck>("datadogruby-without-gvl");
  }
};
// Register the DatadogRubyTidyModule using this statically initialized
// variable.
static ClangTidyModuleRegistry::Add<DatadogRubyModule>
    X("datadog-ruby-module", "Adds checks specific to Datadog's dd-trace-rb");
} // namespace datadogruby

// This anchor is used to force the linker to link in the generated object file
// and thus register the DatadogRubyModule.
volatile int DatadogRubyModuleAnchorSource = 0;

} // namespace clang::tidy
