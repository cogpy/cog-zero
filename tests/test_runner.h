/*
 * standalone/tests/test_runner.h
 *
 * Minimal test framework — zero external deps.
 *
 * testRegistry() is defined in test_main.cpp (a single TU) to avoid ODR issues
 * when this header is included in multiple translation units.
 */
#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

// Declared here, defined in test_main.cpp
std::vector<TestCase>& testRegistry();
int runAllTests(const std::string& filter = {});

// Registration helper — called at static-init time from each TU
inline int registerTest(std::string name, std::function<void()> fn) {
    testRegistry().push_back({std::move(name), std::move(fn)});
    return 0;
}

#define TEST(name) \
    static void test_##name(); \
    static int _reg_##name = registerTest(#name, test_##name); \
    static void test_##name()

#define ASSERT_TRUE(expr) \
    do { if (!(expr)) { \
        std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__ << "] " \
                  << "ASSERT_TRUE(" #expr ")\n"; \
        throw std::runtime_error("assertion failed"); \
    }} while(0)

#define ASSERT_FALSE(expr)  ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b)     ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b)     ASSERT_TRUE((a) != (b))
#define ASSERT_GT(a, b)     ASSERT_TRUE((a) >  (b))
#define ASSERT_GE(a, b)     ASSERT_TRUE((a) >= (b))
#define ASSERT_LT(a, b)     ASSERT_TRUE((a) <  (b))
#define ASSERT_LE(a, b)     ASSERT_TRUE((a) <= (b))

#define ASSERT_NEAR(a, b, eps) \
    do { if (std::fabs((a) - (b)) >= (eps)) { \
        std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__ << "] " \
                  << "ASSERT_NEAR(" #a ", " #b ", " #eps ")" \
                  << " actual diff=" << std::fabs((a) - (b)) << "\n"; \
        throw std::runtime_error("assertion failed"); \
    }} while(0)
