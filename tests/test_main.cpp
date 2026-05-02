/*
 * standalone/tests/test_main.cpp
 *
 * Defines the shared test registry and provides main() for the test binary.
 * Including this in exactly one TU fixes the ODR issue when test_runner.h
 * is included in multiple translation units.
 *
 * Usage:
 *   cog0_tests                     — run all registered tests
 *   cog0_tests --filter <pattern>  — run only tests whose name contains <pattern>
 */
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "test_runner.h"

// Canonical definition — all other TUs call registerTest() which calls this.
std::vector<TestCase>& testRegistry() {
    static std::vector<TestCase> r;
    return r;
}

int runAllTests(const std::string& filter) {
    int passed = 0, failed = 0, skipped = 0;
    for (const auto& tc : testRegistry()) {
        if (!filter.empty() && tc.name.find(filter) == std::string::npos) {
            ++skipped;
            continue;
        }
        try {
            tc.fn();
            std::cout << "  PASS  " << tc.name << "\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cerr << "  FAIL  " << tc.name << " — " << e.what() << "\n";
            ++failed;
        }
    }
    if (skipped > 0)
        std::cout << "  (skipped " << skipped << " tests not matching filter '" << filter << "')\n";
    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
    return (failed == 0) ? 0 : 1;
}

int main(int argc, char** argv) {
    std::string filter;
    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "--filter") == 0 || std::strcmp(argv[i], "-f") == 0)
                && i + 1 < argc) {
            filter = argv[++i];
        }
    }

    std::cout << "=== cog0 Unit Tests ===";
    if (!filter.empty())
        std::cout << "  [filter: " << filter << "]";
    std::cout << "\n\n";

    return runAllTests(filter);
}
