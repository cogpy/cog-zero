#include "test_runner.h"
#include <string>

int main(int argc, char** argv)
{
    std::string filter;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--filter" && i + 1 < argc) {
            filter = argv[++i];
        } else if (arg.rfind("--filter=", 0) == 0) {
            filter = arg.substr(9);
        }
    }
    std::cout << "agentzero-learning Phase 5 tests\n";
    return aztest::run_all(filter);
}
