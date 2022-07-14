#include <fstream>
#include <iostream>

#include "slang/compilation/Compilation.h"
#include "slang/diagnostics/DeclarationsDiags.h"
#include "slang/diagnostics/DiagnosticEngine.h"
#include "slang/diagnostics/ExpressionsDiags.h"
#include "slang/diagnostics/LookupDiags.h"
#include "slang/diagnostics/ParserDiags.h"
#include "slang/diagnostics/SysFuncsDiags.h"
#include "slang/diagnostics/TextDiagnosticClient.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/symbols/ASTSerializer.h"
#include "slang/symbols/CompilationUnitSymbols.h"
#include "slang/symbols/InstanceSymbols.h"
#include "slang/syntax/SyntaxPrinter.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/Json.h"
#include "slang/text/SourceManager.h"
#include "slang/util/CommandLine.h"
#include "slang/util/OS.h"
#include "slang/util/String.h"
#include "slang/util/Version.h"


static constexpr auto noteColor = fmt::terminal_color::bright_black;
static constexpr auto warningColor = fmt::terminal_color::bright_yellow;
static constexpr auto errorColor = fmt::terminal_color::bright_red;
static constexpr auto highlightColor = fmt::terminal_color::bright_green;

slang::SourceBuffer readSource(slang::SourceManager &sourceManager, const std::string &file) {
  slang::SourceBuffer buffer = sourceManager.readSource(slang::widen(file));
  if (!buffer) {
    slang::OS::printE(fg(errorColor), "error: ");
    slang::OS::printE("no such file or directory: '{}'\n", file);
  }
  return buffer;
}

int main(int argc, const char **argv) {
  slang::CommandLine cmdLine;
  std::optional<bool> showHelp;
  std::optional<bool> showVersion;
  cmdLine.add("-h,--help", showHelp, "Display available options");
  cmdLine.add("--version", showVersion, "Display version information and exit");

  // File list
  std::vector<std::string> sourceFiles;
  cmdLine.setPositional(sourceFiles, "files", /* isFileName */ true);

  if (cmdLine.parse(argc, argv)) {
    for (auto &error : cmdLine.getErrors()) {
      std::cerr << error << "\n";
    }
  }

  if (showHelp == true) {
    std::cout << cmdLine.getHelpText("slang tool");
    return 1;
  }

  if (showVersion == true) {
    slang::OS::print("slang version {}.{}.{}\n",
                     slang::VersionInfo::getMajor(),
                     slang::VersionInfo::getMinor(),
                     slang::VersionInfo::getRevision());
    return 1;
  }

  bool anyErrors = false;
  slang::SourceManager sourceManager;

  slang::PreprocessorOptions ppoptions;
  slang::LexerOptions loptions;
  slang::ParserOptions poptions;
  slang::CompilationOptions coptions;

  slang::Bag options;
  options.set(ppoptions);
  options.set(loptions);
  options.set(poptions);
  options.set(coptions);

  std::vector<slang::SourceBuffer> buffers;
  for (const std::string& file : sourceFiles) {
    slang::SourceBuffer buffer = readSource(sourceManager, file);
    if (!buffer) {
      anyErrors = true;
      continue;
    }
    buffers.push_back(buffer);
  }

  if (anyErrors) {
    return 2;
  }

  if (buffers.empty()) {
    slang::OS::printE(fg(errorColor), "error: ");
    slang::OS::printE("no input files\n");
    return 3;
  }

  try {
    // Create compilation objects.
    slang::Compilation compilation(options);
    slang::DiagnosticEngine diagEngine(*compilation.getSourceManager());
    std::shared_ptr<slang::TextDiagnosticClient> diagClient;
    diagClient = std::make_shared<slang::TextDiagnosticClient>();
    diagEngine.addClient(diagClient);
    // Load sources
    for (const slang::SourceBuffer &buffer : buffers) {
      auto tree = slang::SyntaxTree::fromBuffer(buffer, sourceManager, options);
      compilation.addSyntaxTree(tree);
    }
    // Perform compilation.
    for (auto &diag : compilation.getAllDiagnostics()) {
      diagEngine.issue(diag);
    }
    anyErrors |= diagEngine.getNumErrors() != 0;
  } catch (const std::exception& e) {
    slang::OS::printE("internal compiler error: {}\n", e.what());
    return 4;
  }

  return anyErrors ? 1 : 0;
}
