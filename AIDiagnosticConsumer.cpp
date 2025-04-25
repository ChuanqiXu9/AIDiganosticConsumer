//===- AIDiagnosticConsumer.cpp --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AIDiagnosticConsumer.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/JSON.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <cstdlib>
#include <curl/curl.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <memory>

using namespace clang;

namespace {
struct MemoryStruct {
  char *response = nullptr;
  size_t size = 0;
};

size_t write_callback(char *data, size_t size, size_t nmemb, void *userdata) {
  size_t realsize = size * nmemb;
  auto *mem = (struct MemoryStruct *)userdata;
 
  void *ptr = realloc(mem->response, mem->size + realsize + 1);
  if(!ptr)
    return 0;  /* out of memory */
 
  mem->response = (char*)ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;
 
  return realsize;
}

class AIDiagnosticConsumer : public DiagnosticConsumer {
public:
  AIDiagnosticConsumer(CompilerInstance &CI) : CI(CI) {
    Init();
  }

  void Init();

  void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                        const Diagnostic &Info) override;

  void getPrompt(std::string &Prompt, const Diagnostic &Info);

private:
  const char *AK = nullptr;
  const char *Model = nullptr;
  const char *REPLY_LANG = nullptr;
  const char *ROLE_PROMPT = nullptr;
  llvm::SmallString<512> RolePrompt;
  llvm::SmallString<64> StdlibVersion;
  CompilerInstance &CI;
};

template <typename... T> static void elog(const char *Fmt, T &&... Vals) {
  llvm::errs().changeColor(llvm::raw_ostream::RED);
  llvm::errs() << llvm::formatv(Fmt, std::forward<T>(Vals)...) << "\n";
  llvm::errs().resetColor();
}

void AIDiagnosticConsumer::Init() {
  AK = std::getenv("CLANG_AI_KEY");
  if (!AK) {
    elog("FAILED to find API key for AI in clang. Please set the API key in the environment variable CLANG_AI_KEY.");
    return;
  }

  Model = std::getenv("CLANG_AI_MODEL");
  if (!Model)
    Model = "qwen-max";

  REPLY_LANG = std::getenv("CLANG_AI_REPLY_LANG");
  if (!REPLY_LANG)
    REPLY_LANG = "中文";

  ROLE_PROMPT = std::getenv("CLANG_AI_ROLE_PROMPT");
  if (!ROLE_PROMPT)
    ROLE_PROMPT = "You're an AI assistant that helps improve compiler errors. "
                  "Your task is to analyze the given error message and provide a solution. "
                  "Don't repear error message simply. "
                  "Don't guess if you're not sure about your reply. "
                  "Please be brief as much as possible.";

  RolePrompt = ROLE_PROMPT;
  RolePrompt += "Please reply in ";
  RolePrompt += REPLY_LANG;
  RolePrompt += ". ";

  RolePrompt += "Please translate the error message if you were asked to reply in language other than English. ";

  // Get information from preprocessor
  if (!CI.hasPreprocessor())
    return;

  auto &PP = CI.getPreprocessor();

  auto GetLiteralMacroValue = [&PP](StringRef MacroName) -> std::optional<llvm::SmallString<64>> {
    // Might not be good since IdentifierTable::get may add additional identifiers.
    IdentifierTable &Table = const_cast<Preprocessor &>(PP).getIdentifierTable();
    auto &IInfo = Table.get(MacroName);
    auto *MInfo = PP.getMacroInfo(&IInfo);
    if (!MInfo)
      return std::nullopt;

    if (!MInfo->getNumTokens())
      return std::nullopt;
    
    auto ValueToken = MInfo->getReplacementToken(0);
    if (!ValueToken.isLiteral())
      return std::nullopt;

    return llvm::SmallString<64>(ValueToken.getLiteralData());
  };

  // FIXME: This seems to not be good.
  if (auto StdLibCXXVersion = GetLiteralMacroValue("_GLIBCXX_RELEASE")) {
    StdlibVersion = "libstdc++ ";
    StdlibVersion += *StdLibCXXVersion;
  } else if (auto LibCXXVersion = GetLiteralMacroValue("_LIBCPP_VERSION")) {
    StdlibVersion = "libc++ ";
    StdlibVersion += *LibCXXVersion;
  }
}

void AIDiagnosticConsumer::getPrompt(std::string &Prompt, const Diagnostic &Info) {
  llvm::SmallString<256> OutStr;
  Info.FormatDiagnostic(OutStr);

  llvm::raw_string_ostream OS(Prompt);
  auto &SrcMgr = Info.getSourceManager();
  auto SpellingLoc = SrcMgr.getSpellingLoc(Info.getLocation());

  std::string FileName;
  llvm::raw_string_ostream FileOS(FileName);
  SpellingLoc.print(FileOS, SrcMgr);

  OS << "Error Message: '" << FileName << "': " << OutStr << ". ";
  OS << "The error message is produced by Clang. ";
  if (!StdlibVersion.empty())
    OS << "The used standard library is " << StdlibVersion << ". ";

  if (!CI.hasSema())
    return;

  auto &S = CI.getSema();
  // If we are in the process of template instantiation, we can print the
  // instantiation stack to help the AI to understand the context.
  // This is the general too verbose part for human to understand.
  if (S.inTemplateInstantiation()) {
    // FIXME: The folloing code doens't print the enclosing declaration before instantiation.
    // e.g.,
    //
    // void func() { std::unique_ptr<int> p = std::make_unique<int>(1); }
    //
    // I hope to print func as an input to AI.

    
    auto Iter = S.CodeSynthesisContexts.begin(), End = S.CodeSynthesisContexts.end(); 
    if (Iter == End)
      return;

    OS << "We're in the process of a template instantiation. The following were the instantiation stack: \n";
    OS << "The instantiation is triggered by: \n";
    Iter->PointOfInstantiation.print(OS, SrcMgr);
    OS << "\n";

    // FIXME: print a file may be too verbose. See the above FIXME.
    auto FID = SrcMgr.getFileID(Iter->PointOfInstantiation);
    if (std::optional<StringRef> Filebuffer = SrcMgr.getBufferDataOrNone(FID)) {
      OS << "The file containing the instantiation point: " << Filebuffer;
    }
    // How can we get the enclosing function by the location?

    for (;Iter != End; ++Iter) {
      OS << "\n";
      OS << Lexer::getSourceText(SrcMgr.getExpansionRange(Iter->Entity->getSourceRange()),
                           SrcMgr,
                           CI.getLangOpts());
      OS << "\n";
    }
  } else {
    if (DeclContext *CurDC = S.getCurLexicalContext()) {
      OS << "The current parsing context is: ";
      OS << Lexer::getSourceText(SrcMgr.getExpansionRange(cast<Decl>(CurDC)->getSourceRange()),
                            SrcMgr,
                            CI.getLangOpts());

      // If the current function is a method, maybe it is good to print the class.
    }
  }
}

void AIDiagnosticConsumer::HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                                                   const Diagnostic &Info) {
  if (!AK)
    return;

  if (DiagLevel <= DiagnosticsEngine::Warning)
    return;

  std::string Prompt;
  getPrompt(Prompt, Info);
  
  CURL *curl;

  curl = curl_easy_init();
  if (curl) {
      curl_easy_setopt(curl, CURLOPT_URL, "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions");

      struct curl_slist *headers = NULL;
      llvm::SmallString<256> Authorization("Authorization: Bearer ");
      Authorization += AK;
		  headers = curl_slist_append(headers, Authorization.c_str());
		  headers = curl_slist_append(headers, "Content-Type: application/json");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

      llvm::json::Object Request{{"model", Model}, {"enable_search", true}};
      llvm::json::Object Message;
      llvm::json::Array Messages;
      Messages.push_back(llvm::json::Object({{"role", "system"}, {"content", RolePrompt.c_str()}}));
      Messages.push_back(llvm::json::Object({{"role", "user"}, {"content", Prompt.c_str()}}));
      Request.insert({"messages", std::move(Messages)});
      llvm::json::Object SearchOptions;
      SearchOptions.insert({"forced_search", true});
      Request.insert({"search_options", std::move(SearchOptions)});

      std::string RequestData;
      llvm::raw_string_ostream OS(RequestData);
      OS << llvm::json::Value(std::move(Request));
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, RequestData.c_str());

      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
      MemoryStruct Response;
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &Response);
      CURLcode res = curl_easy_perform(curl);
      if (res == CURLE_OK) {
        [](StringRef Response) {
          llvm::Expected<llvm::json::Value> ExpectedResponseJson = llvm::json::parse(Response);
          if (llvm::Error Err = ExpectedResponseJson.takeError()) {
            elog("AI response error: %0\n", llvm::toString(std::move(Err)));
            return;
          }

          if (ExpectedResponseJson->getAsObject()->getObject("error")) {
            elog("AI Response Error: %0\n", Response);
            return;
          } 
          
          auto *Array = ExpectedResponseJson->getAsObject()->getArray("choices");
          for (auto &Choice : *Array) {
            if (std::optional<StringRef> Content = Choice.getAsObject()->getObject("message")->getString("content")) {
              elog("AI Suggestion: ");
              llvm::errs() << *Content << "\n";
            }
          }
        }(Response.response);
      }

      free(Response.response);
      curl_easy_cleanup(curl);
  }
}
}

namespace  clang {
std::unique_ptr<DiagnosticConsumer> createAIDiagnosticConsumer(
    CompilerInstance &CI) {
  return std::make_unique<AIDiagnosticConsumer>(CI);
}
}
