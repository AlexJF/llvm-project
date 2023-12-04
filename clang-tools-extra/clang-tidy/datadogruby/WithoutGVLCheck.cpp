//===--- WithoutGVLCheck.cpp - clang-tidy ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "WithoutGVLCheck.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Regex.h"
#include <iostream>

using namespace clang;
using namespace clang::ast_matchers;
using namespace std::string_literals;

namespace clang::tidy::datadogruby {

const auto AnnotationGVLGuard = "datadog_ruby_gvl_guard";
const auto AnnotationNoGVLSafe = "datadog_ruby_nogvl_safe";

WithoutGVLCheck::WithoutGVLCheck(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context) {
  NoGVLSafeWhitelist.emplace_back("^pthread_.+$");
  NoGVLSafeWhitelist.emplace_back("^__builtin.+$");
  NoGVLSafeWhitelist.emplace_back("^rb_thread_call_with_gvl$");
  NoGVLSafeWhitelist.emplace_back("^ruby_vsnprintf$");
  NoGVLSafeWhitelist.emplace_back("^rb_bug$");
  NoGVLSafeWhitelist.emplace_back("^rb_thread_(current|main)$");
  NoGVLSafeWhitelist.emplace_back("^ddog_prof_.+$");
}

void WithoutGVLCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      traverse(TK_IgnoreUnlessSpelledInSource,
               callExpr(stmt(callExpr().bind("potentially_dangerous_call")),
                        hasAncestor(functionDecl(hasAttr(attr::Annotate))
                                        .bind("maybe_safe_context")),
                        optionally(hasAncestor(ifStmt(
                            hasCondition(anyOf(declRefExpr().bind("guard_var"),
                                               callExpr().bind("guard_call"))),
                            hasThen(hasDescendant(callExpr(equalsBoundNode(
                                "potentially_dangerous_call"))))))))),
      this);
  Finder->addMatcher(
      traverse(TK_IgnoreUnlessSpelledInSource,
               callExpr(allOf(callee(functionDecl(
                                  matchesName("rb_thread_call_without_gvl"))),
                              hasArgument(0, declRefExpr(to(functionDecl().bind(
                                                 "func_without_gvl"))))))
                   .bind("transition_without_gvl")),
      this);
}

bool WithoutGVLCheck::isGVLSafeFunction(const FunctionDecl *Function) {
  if (!Function) {
    // not a valid function?, be cautious and assume not GVL safe.
    return false;
  }

  for (const auto *A : Function->specific_attrs<AnnotateAttr>()) {
    if (A->getAnnotation() == AnnotationNoGVLSafe) {
      return true;
    }
    if (A->getAnnotation() == AnnotationGVLGuard) {
      // guards are implicit safe
      return true;
    }
  }

  const auto Name = Function->getName();

  for (const llvm::Regex &R : NoGVLSafeWhitelist) {
    if (R.match(Name)) {
      return true;
    }
  }

  return false;
}

bool WithoutGVLCheck::isGVLGuardDecl(const Decl *PotentialGVLGuardDecl) {
  if (!PotentialGVLGuardDecl) {
    // not a valid declaration, be cautious and assume not a guard
    return false;
  }

  for (const auto *A : PotentialGVLGuardDecl->specific_attrs<AnnotateAttr>()) {
    if (A->getAnnotation() == AnnotationGVLGuard) {
      return true;
    }
  }

  // TODO: Alternative to annotations

  return false;
}

void WithoutGVLCheck::diagGVLUnsafeCall(const FunctionDecl *SafeContext,
                                        const CallExpr *UnsafeCall) {
  const FunctionDecl *UnsafeFunction = UnsafeCall->getDirectCallee();

  diag(UnsafeCall->getExprLoc(),
       "Call to NoGVL unsafe function '%0' in a NoGVL safe context",
       DiagnosticIDs::Error)
      << UnsafeFunction->getName() << UnsafeCall->getSourceRange();
  diag(UnsafeCall->getExprLoc(),
       "Check if '%0' is NoGVL safe and annotate it as such, look for an "
       "alternative or protect its usage with a GVL_GUARD if",
       DiagnosticIDs::Note)
      << UnsafeFunction->getNameAsString() << UnsafeCall->getSourceRange();
  diag(UnsafeFunction->getLocation(),
       "Function is being called from a NoGVL safe "
       "context in '%0' but not marked as NoGVL safe",
       DiagnosticIDs::Error)
      << SafeContext->getName() << UnsafeCall->getExprLoc();
}

void WithoutGVLCheck::checkPotentiallyDangerousCall(
    const MatchFinder::MatchResult &Result,
    const CallExpr *PotentiallyDangerousCall) {
  if (!PotentiallyDangerousCall) {
    // No such functions matched
    return;
  }

  const FunctionDecl *MaybeSafeContext =
      Result.Nodes.getNodeAs<FunctionDecl>("maybe_safe_context");

  if (!isGVLSafeFunction(MaybeSafeContext)) {
    // Context isn't NoGVL safe so call is not considered dangerous for this
    // check
    return;
  }

  // Context is NoGVL safe. This call must be to a NoGVL safe function for it
  // not to be dangerous
  const FunctionDecl *CalledFunction =
      PotentiallyDangerousCall->getDirectCallee();

  if (isGVLSafeFunction(CalledFunction)) {
    // NoGVL safe function called in gvl safe context, all is good...
    return;
  }

  // Even if a call is made to a GVL unsafe function, it is not dangerous if
  // it's done in the context of a GVL guard. Lets try and check for this.
  // Example:
  //
  // NOGVL_SAFE void f() {
  //   if (is_current_thread_holding_the_gvl()) {
  //     rb_raise("I can raise here since I actually hold the GVL");
  //   }
  // }
  const DeclRefExpr *GuardVar =
      Result.Nodes.getNodeAs<DeclRefExpr>("guard_var");
  const CallExpr *GuardCall = Result.Nodes.getNodeAs<CallExpr>("guard_call");
  if (GuardVar || GuardCall) {
    const Decl *GuardDecl =
        GuardVar ? GuardVar->getFoundDecl() : GuardCall->getDirectCallee();
    if (isGVLGuardDecl(GuardDecl)) {
      return;
    }
  }

  diagGVLUnsafeCall(MaybeSafeContext, PotentiallyDangerousCall);
}

void WithoutGVLCheck::checkTransitionWithoutGVL(
    const MatchFinder::MatchResult &Result,
    const CallExpr *TransitionWithoutGVL) {
  const FunctionDecl *FunctionWithoutGVL =
      Result.Nodes.getNodeAs<FunctionDecl>("func_without_gvl");

  if (isGVLSafeFunction(FunctionWithoutGVL)) {
    return;
  }

  diag(TransitionWithoutGVL->getArg(0)->getExprLoc(),
       "Calling a NoGVL unsafe function '%0' while transitioning to not "
       "holding the GVL",
       DiagnosticIDs::Error)
      << FunctionWithoutGVL->getName()
      << TransitionWithoutGVL->getSourceRange();
  diag(TransitionWithoutGVL->getArg(0)->getExprLoc(),
       "Check if '%0' is NoGVL safe and annotate it as such or use an "
       "alternative.",
       DiagnosticIDs::Note)
      << FunctionWithoutGVL->getNameAsString()
      << TransitionWithoutGVL->getSourceRange();
  diag(FunctionWithoutGVL->getLocation(),
       "Function is being called from a NoGVL safe "
       "context in '%0' but not marked as NoGVL safe",
       DiagnosticIDs::Error)
      << TransitionWithoutGVL->getDirectCallee()->getName()
      << TransitionWithoutGVL->getExprLoc();
}

void WithoutGVLCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *PotentiallyDangerousCall =
      Result.Nodes.getNodeAs<CallExpr>("potentially_dangerous_call");
  if (PotentiallyDangerousCall) {
    checkPotentiallyDangerousCall(Result, PotentiallyDangerousCall);
    return;
  }

  const auto *TransitionWithoutGVL =
      Result.Nodes.getNodeAs<CallExpr>("transition_without_gvl");
  if (TransitionWithoutGVL) {
    checkTransitionWithoutGVL(Result, TransitionWithoutGVL);
    return;
  }
}

} // namespace clang::tidy::datadogruby
