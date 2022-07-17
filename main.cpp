#include "slang/driver/Driver.h"

#include <fmt/color.h>
#include <fstream>
#include <iostream>

#include "slang/ast/ASTSerializer.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/diagnostics/TextDiagnosticClient.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxVisitor.h"
#include "slang/text/Json.h"
#include "slang/util/TimeTrace.h"
#include "slang/util/Version.h"
#include "slang/ast/ASTVisitor.h"

using namespace slang;
using namespace slang::ast;
using namespace slang::driver;

class ToolVisitor : public ASTVisitor<ToolVisitor, true, false> {
public:
  ToolVisitor() {}

  void handle(const VariableSymbol &symbol) {
    std::string path;
    symbol.getHierarchicalPath(path);
    std::cout << "Visited Variable " << path << "\n";
    if (symbol.getFirstDriver()) {
      const Symbol *driverSymbol = symbol.getFirstDriver()->containingSymbol;
      std::string driverPath;
      driverSymbol->getHierarchicalPath(driverPath);
      std::cout << "First driver " << driverPath << "\n";
    }
  }
};

void writeToFile(string_view fileName, string_view contents);

void printJson(Compilation& compilation, const std::string& fileName,
               const std::vector<std::string>& scopes) {
    JsonWriter writer;
    writer.setPrettyPrint(true);

    ASTSerializer serializer(compilation, writer);
    if (scopes.empty()) {
        serializer.serialize(compilation.getRoot());
    }
    else {
        for (auto& scopeName : scopes) {
            auto sym = compilation.getRoot().lookupName(scopeName);
            if (sym)
                serializer.serialize(*sym);
        }
    }

    writeToFile(fileName, writer.view());
}

template<typename Stream, typename String>
void writeToFile(Stream& os, string_view fileName, String contents) {
    os.write(contents.data(), contents.size());
    os.flush();
    if (!os)
        throw std::runtime_error(fmt::format("Unable to write AST to '{}'", fileName));
}

void writeToFile(string_view fileName, string_view contents) {
    if (fileName == "-") {
        writeToFile(std::cout, "stdout", contents);
    }
    else {
        std::ofstream file{std::string(fileName)};
        writeToFile(file, fileName, contents);
    }
}

int main(int argc, const char **argv) {

  Driver driver;
  driver.addStandardArgs();

  std::optional<bool> showHelp;
  std::optional<bool> showVersion;
  std::optional<bool> quiet;
  std::optional<bool> dumpJson;
  driver.cmdLine.add("-h,--help",   showHelp,    "Display available options and exit");
  driver.cmdLine.add("--version",   showVersion, "Display version information and exit");
  driver.cmdLine.add("-q,--quiet",  quiet,       "Suppress non-essential output");

  std::optional<std::string> astJsonFile;
  driver.cmdLine.add("--ast-json", astJsonFile,
                     "Dump the compiled AST in JSON format to the specified file, or '-' for stdout", "<file>",
                     /* isFileName */ true);

  std::vector<std::string> astJsonScopes;
  driver.cmdLine.add("--ast-json-scope", astJsonScopes,
                     "When dumping AST to JSON, include only the scopes specified by the "
                     "given hierarchical paths",
                     "<path>");

  if (!driver.parseCommandLine(argc, argv)) {
    return 1;
  }

  if (showHelp == true) {
    printf("%s\n", driver.cmdLine.getHelpText("slang SystemVerilog compiler").c_str());
    return 0;
  }

  if (showVersion == true) {
    printf("slang version %d.%d.%d+%s\n", VersionInfo::getMajor(),
        VersionInfo::getMinor(), VersionInfo::getPatch(),
        std::string(VersionInfo::getHash()).c_str());
    return 0;
  }

  if (!driver.processOptions()) {
    return 2;
  }

  bool ok = driver.parseAllSources();

  auto compilation = driver.createCompilation();
  ok &= driver.reportCompilation(*compilation, quiet == true);

  if (!ok) {
    return ok;
  }

  if (astJsonFile) {
    printJson(*compilation, *astJsonFile, astJsonScopes);
    return 0;
  }

  ToolVisitor visitor;
  compilation->getRoot().visit(visitor);

  return 0;
}
