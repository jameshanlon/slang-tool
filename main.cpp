#include <fstream>
#include <iostream>

#include <slang/util/CommandLine.h>

int main(int argc, const char **argv) {
  slang::CommandLine cmdLine;
  optional<bool> showHelp;
  cmdLine.add("-h,--help", showHelp, "Display available options");

  if (cmdLine.parse(argc, argv)) {
    for (auto &error : cmdLine.getErrors()) {
      std::cerr << error << "\n";
    }
  }

  if (showHelp == true) {
    std::cout << cmdLine.getHelpText("slang tool");
    return 1;
  }

  std::cout << "hello\n";
  return 0;
}
