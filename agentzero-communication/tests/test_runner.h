/*
 * Minimal zero-dependency test runner for agentzero-communication.
 * Compatible in spirit with agentzero-core/tests/test_runner.h
 */
#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace aztest {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

struct AssertFail : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline void expect(bool cond, const std::string& msg, const char* file, int line) {
    if (!cond) {
        throw AssertFail(std::string(file) + ":" + std::to_string(line) + " " + msg);
    }
}

inline int run_all(const std::string& filter = "") {
    int passed = 0, failed = 0, skipped = 0;
    for (const auto& t : registry()) {
        if (!filter.empty() && t.name.find(filter) == std::string::npos) {
            ++skipped;
            continue;
        }
        try {
            t.fn();
            std::cout << "  PASS  " << t.name << "\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cout << "  FAIL  " << t.name << " — " << e.what() << "\n";
            ++failed;
        }
    }
    std::cout << "\nResults: " << passed << " passed, " << failed
              << " failed, " << skipped << " skipped\n";
    return failed == 0 ? 0 : 1;
}

} // namespace aztest

#define TEST(name) \
    static void test_##name(); \
    static aztest::Registrar registrar_##name(#name, test_##name); \
    static void test_##name()

#define ASSERT_TRUE(x)  aztest::expect(static_cast<bool>(x), "ASSERT_TRUE(" #x ")", __FILE__, __LINE__)
#define ASSERT_FALSE(x) aztest::expect(!(x), "ASSERT_FALSE(" #x ")", __FILE__, __LINE__)
#define ASSERT_EQ(a,b)  aztest::expect((a)==(b), "ASSERT_EQ(" #a "," #b ")", __FILE__, __LINE__)
#define ASSERT_NE(a,b)  aztest::expect((a)!=(b), "ASSERT_NE(" #a "," #b ")", __FILE__, __LINE__)
#define ASSERT_GT(a,b)  aztest::expect((a)>(b), "ASSERT_GT(" #a "," #b ")", __FILE__, __LINE__)
#define ASSERT_GE(a,b)  aztest::expect((a)>=(b), "ASSERT_GE(" #a "," #b ")", __FILE__, __LINE__)
#define ASSERT_LT(a,b)  aztest::expect((a)<(b), "ASSERT_LT(" #a "," #b ")", __FILE__, __LINE__)
#define ASSERT_LE(a,b)  aztest::expect((a)<=(b), "ASSERT_LE(" #a "," #b ")", __FILE__, __LINE__)
#define ASSERT_STREQ(a,b) aztest::expect(std::string(a)==std::string(b), "ASSERT_STREQ", __FILE__, __LINE__)
#define ASSERT_NEAR(a,b,eps) aztest::expect(std::fabs((double)(a)-(double)(b))<=(eps), "ASSERT_NEAR(" #a "," #b ")", __FILE__, __LINE__)
