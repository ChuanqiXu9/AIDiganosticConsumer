//===- AIDiagnosticConsumer.h ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include <memory>

namespace clang {
std::unique_ptr<DiagnosticConsumer> createAIDiagnosticConsumer(
    CompilerInstance &CI);
}
