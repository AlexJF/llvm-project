//===--- DangerousInteractionsCheck.cpp - clang-tidy ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "DangerousInteractionsCheck.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace std::string_literals;

namespace clang::tidy::datadogruby {

const auto AnnotationGcSafe = "datadog_ruby_gc_safe";
const auto AnnotationNoGVLSafe = "datadog_ruby_nogvl_safe";

void DangerousInteractionsCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      callExpr(hasAncestor(functionDecl(hasAttr(attr::Annotate))
                               .bind("potentially_dangerous_context")))
          .bind("suspicious_call"),
      this);
}

void DangerousInteractionsCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *CallingContext =
      Result.Nodes.getNodeAs<FunctionDecl>("potentially_dangerous_context");
  const auto *MatchedCall = Result.Nodes.getNodeAs<CallExpr>("suspicious_call");

  bool ContextGcSafe = false;
  bool ContextNoGVLSafe = false;

  for (const auto *A : CallingContext->specific_attrs<AnnotateAttr>()) {
    if (A->getAnnotation() == AnnotationGcSafe) {
      ContextGcSafe = true;
    } else if (A->getAnnotation() == AnnotationNoGVLSafe) {
      ContextNoGVLSafe = true;
    }
  }

  if (!ContextGcSafe && !ContextNoGVLSafe) {
    // Not a dangerous context afterall
    return;
  }

  const auto *CalledFunc = MatchedCall->getDirectCallee();

  bool CallGcSafe = false;
  bool CallNoGVLSafe = false;

  for (const auto *A : CalledFunc->specific_attrs<AnnotateAttr>()) {
    if (A->getAnnotation() == AnnotationGcSafe) {
      CallGcSafe = true;
    } else if (A->getAnnotation() == AnnotationNoGVLSafe) {
      CallNoGVLSafe = true;
    }
  }

  if (ContextGcSafe && !CallGcSafe) {
    diag(MatchedCall->getExprLoc(),
         "call to GC unsafe function '%0' in a GC safe context",
         DiagnosticIDs::Error)
        << CalledFunc->getName() << MatchedCall->getSourceRange();
    diag(MatchedCall->getExprLoc(),
         "Check if '%0' is GC safe and annotate it as such or use an "
         "alternative",
         DiagnosticIDs::Note)
        << CalledFunc->getName() << MatchedCall->getSourceRange();
    diag(CalledFunc->getLocation(),
         "Function is being called from a GC safe "
         "context in '%0' but not marked as GC safe",
         DiagnosticIDs::Error)
        << CallingContext->getName() << MatchedCall->getExprLoc();
  }

  if (ContextNoGVLSafe && !CallNoGVLSafe) {
    diag(MatchedCall->getExprLoc(),
         "call to NoGVL unsafe function '%0' in a NoGVL safe context",
         DiagnosticIDs::Error)
        << CalledFunc->getName() << MatchedCall->getSourceRange();
    diag(MatchedCall->getExprLoc(),
         "Check if '%0' is NoGVL safe and annotate it as such or use an "
         "alternative",
         DiagnosticIDs::Note)
        << CalledFunc->getNameAsString() << MatchedCall->getSourceRange();
    diag(CalledFunc->getLocation(),
         "Function is being called from a NoGVL safe "
         "context in '%0' but not marked as NoGVL safe",
         DiagnosticIDs::Error)
        << CallingContext->getName() << MatchedCall->getExprLoc();
  }
}

} // namespace clang::tidy::datadogruby
