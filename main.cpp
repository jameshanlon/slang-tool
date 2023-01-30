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

/*class ForLoopUnrollVisitor : public ASTVisitor<ForLoopUnrollVisitor, true, false> {
  EvalContext evalCtx;
  Compilation &compilation;

public:
  bool anyErrors = false;

  ForLoopUnrollVisitor(Compilation &compilation) : compilation(compilation) {
    evalCtx.pushEmptyFrame();
  }

  using ER = ast::Statement::EvalResult;

  //void handle(const VariableSymbol &symbol) {
  //  std::string path;
  //  symbol.getHierarchicalPath(path);
  //  std::cout << "Visited Variable " << path << "\n";
  //  auto driver = symbol.getFirstDriver();
  //  while (driver != nullptr) {
  //    const Symbol *driverSymbol = symbol.getFirstDriver()->containingSymbol;
  //    std::string driverPath;
  //    driverSymbol->getHierarchicalPath(driverPath);
  //    std::cout << "Driver " << driverPath << "\n";
  //    driver = driver->getNextDriver();
  //  }
  //}

  //ER evalForLoopStatement(const ForLoopStatement &stmt) {
  //  EvalContext context(compilation);
  //  for (auto init : stmt.initializers) {
  //    if (!init->eval(context)) {
  //      std::cout << "Initialiser fail\n";
  //      return ER::Fail;
  //    }
  //  }
  //  while (true) {
  //    if (stmt.stopExpr) {
  //      auto result = stmt.stopExpr->eval(context);
  //      if (result.bad()) {
  //        std::cout << "Stop fail\n";
  //        return ER::Fail;
  //      }
  //      if (!result.isTrue()) {
  //        break;
  //      }
  //    }
  //    std::cout << "Loop body\n";
  //    for (auto step : stmt.steps) {
  //      if (!step->eval(context)) {
  //        std::cout << "Step fail\n";
  //        return ER::Fail;
  //      }
  //    }
  //  }
  //  return ER::Success;
  //}

  void handle (const ForLoopStatement &loop) {
    std::cout << "ForLoopStatement\n";
    //auto result = evalForLoopStatement(stmt);
    //std::cout << "Result " << (int)result << "\n";

    if (loop.loopVars.empty() || !loop.stopExpr || loop.steps.empty() || anyErrors) {
      loop.body.visit(*this);
      return;
    }
        
    // Attempt to unroll the loop. If we are unable to collect constant values
    // for all loop variables across all iterations, we won't unroll at all.
    auto handleFail = [&] {
      for (auto var : loop.loopVars) {
        evalCtx.deleteLocal(var);
      }
      loop.body.visit(*this);
    };

    // Loop variables.
    SmallVector<ConstantValue*> localPtrs;
    for (auto var : loop.loopVars) {
      auto init = var->getInitializer();
      if (!init) {
          handleFail();
          return;
      }

      auto cv = init->eval(evalCtx);
      if (!cv) {
          handleFail();
          return;
      }

      localPtrs.push_back(evalCtx.createLocal(var, std::move(cv)));
    }

    // Loop iterations.
    SmallVector<ConstantValue, 16> values;
    while (true) {
        auto cv = step() ? loop.stopExpr->eval(evalCtx) : ConstantValue();
        if (!cv) {
            handleFail();
            return;
        }

        if (!cv.isTrue())
            break;

        for (auto local : localPtrs)
            values.emplace_back(*local);

        for (auto step : loop.steps) {
            if (!step->eval(evalCtx)) {
                handleFail();
                return;
            }
        }
    }

  }
};*/

class UnrollVisitor : public ASTVisitor<UnrollVisitor, true, false> {
public:
    bool anyErrors = false;

    explicit UnrollVisitor(Compilation &compilation) :
        evalCtx(compilation) {
        evalCtx.pushEmptyFrame();
    }

    void handle(const ForLoopStatement& loop) {
        if (loop.loopVars.empty() || !loop.stopExpr || loop.steps.empty() || anyErrors) {
            loop.body.visit(*this);
            return;
        }

        // Attempt to unroll the loop. If we are unable to collect constant values
        // for all loop variables across all iterations, we won't unroll at all.
        auto handleFail = [&] {
            for (auto var : loop.loopVars)
                evalCtx.deleteLocal(var);
            loop.body.visit(*this);
        };

        SmallVector<ConstantValue*> localPtrs;
        for (auto var : loop.loopVars) {
            auto init = var->getInitializer();
            if (!init) {
                handleFail();
                return;
            }

            auto cv = init->eval(evalCtx);
            if (!cv) {
                handleFail();
                return;
            }

            localPtrs.push_back(evalCtx.createLocal(var, std::move(cv)));
        }

        SmallVector<ConstantValue, 16> values;
        while (true) {
            auto cv = step() ? loop.stopExpr->eval(evalCtx) : ConstantValue();
            if (!cv) {
                handleFail();
                return;
            }

            if (!cv.isTrue())
                break;

            for (auto local : localPtrs)
                values.emplace_back(*local);

            for (auto step : loop.steps) {
                if (!step->eval(evalCtx)) {
                    handleFail();
                    return;
                }
            }
        }

        // We have all the loop iteration values. Go back through
        // and visit the loop body for each iteration.
        for (size_t i = 0; i < values.size();) {
            for (auto local : localPtrs)
                *local = std::move(values[i++]);

            std::cout << "visit body\n";
            loop.body.visit(*this);
            if (anyErrors)
                return;
        }
    }

    void handle(const ConditionalStatement& stmt) {
        std::cout << "ConditionalStatement\n";
        // Evaluate the condition; if not constant visit both sides,
        // otherwise visit only the side that matches the condition.
        auto fallback = [&] {
            stmt.ifTrue.visit(*this);
            if (stmt.ifFalse)
                stmt.ifFalse->visit(*this);
        };

        for (auto& cond : stmt.conditions) {
            if (cond.pattern || !step()) {
                fallback();
                return;
            }

            auto result = cond.expr->eval(evalCtx);
            if (!result) {
                fallback();
                return;
            }

            if (!result.isTrue()) {
                if (stmt.ifFalse)
                    stmt.ifFalse->visit(*this);
                return;
            }
        }

        stmt.ifTrue.visit(*this);
    }

    void handle(const ExpressionStatement& stmt) {
        std::cout << "ExpressionStatement\n";
        step();
        //if (stmt.expr.kind == ExpressionKind::Assignment) {
        //    auto& assign = stmt.expr.as<AssignmentExpression>();
        //    auto flags = assign.isNonBlocking() ? AssignFlags::NonBlocking : AssignFlags::None;
        //    anyErrors |= !assign.left().requireLValue(astCtx, {}, flags, nullptr, &evalCtx);
        //}
    }

private:
    bool step() {
        if (anyErrors || !evalCtx.step(SourceLocation::NoLocation)) {
            anyErrors = true;
            return false;
        }
        return true;
    }

    //ASTContext astCtx;
    EvalContext evalCtx;
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

  UnrollVisitor visitor(*compilation);
  compilation->getRoot().visit(visitor);

  return 0;
}
