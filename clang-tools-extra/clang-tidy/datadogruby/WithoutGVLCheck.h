//===--- DangerousInteractionsCheck.h - clang-tidy --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_DATADOGRUBY_WITHOUTGVLCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_DATADOGRUBY_WITHOUTGVLCHECK_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::datadogruby {

/// FIXME: Write a short description.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/datadogruby/dangerous-interactions.html
class WithoutGVLCheck : public ClangTidyCheck {
public:
  WithoutGVLCheck(StringRef Name, ClangTidyContext *Context);

  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

private:
  bool isGVLSafeFunction(const FunctionDecl *Function);
  bool isGVLGuardDecl(const Decl *PotentialGVLGuardDecl);
  void checkPotentiallyDangerousCall(
      const ast_matchers::MatchFinder::MatchResult &Result,
      const CallExpr *PotentiallyDangerousCall);
  void checkTransitionWithoutGVL(
      const ast_matchers::MatchFinder::MatchResult &Result,
      const CallExpr *TransitionWithoutGVL);
  void diagGVLUnsafeCall(const FunctionDecl *SafeContext,
                         const CallExpr *UnsafeCall);
  std::vector<llvm::Regex> NoGVLSafeWhitelist;
};

} // namespace clang::tidy::datadogruby

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_DATADOGRUBY_WITHOUTGVLCHECK_H
