#include "Commands.h"
#include <iostream>

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
        printUsage();
        return 0;
    }
    std::string name = args[0];
    args.erase(args.begin());
    return runCommand(name, args, true, std::cerr);
}
