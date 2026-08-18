/*
 * standalone/tests/bench_runner.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Minimal benchmark framework — zero external deps, C++17 stdlib only.
 *
 * Usage:
 *   BENCHMARK(my_bench) {
 *       // code to time
 *   }
 *
 * Each benchmark is called BENCH_ITERATIONS times (default 1000).
 * Wall-clock time is measured with std::chrono::steady_clock.
 * Results are printed as: name | iterations | total_ms | mean_us | throughput
 */
#pragma once

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Default number of iterations per benchmark (may be overridden per benchmark)
#ifndef BENCH_ITERATIONS
#  define BENCH_ITERATIONS 1000
#endif

// ---------------------------------------------------------------------------
// BenchCase — a named benchmark with a callable

struct BenchCase {
    std::string name;
    size_t      iterations;
    std::function<void()> fn;
};

// Registry — defined in bench_main.cpp (single TU); declared here for all TUs.
std::vector<BenchCase>& benchRegistry();

inline int registerBench(std::string name, size_t iters, std::function<void()> fn) {
    benchRegistry().push_back({std::move(name), iters, std::move(fn)});
    return 0;
}

// ---------------------------------------------------------------------------
// BENCHMARK macro — registers and defines a benchmark function.
// Optional second argument specifies iteration count.

#define BENCHMARK(bname) \
    static void bench_##bname(); \
    static int _breg_##bname = registerBench(#bname, BENCH_ITERATIONS, bench_##bname); \
    static void bench_##bname()

#define BENCHMARK_N(bname, iters) \
    static void bench_##bname(); \
    static int _breg_##bname = registerBench(#bname, (iters), bench_##bname); \
    static void bench_##bname()

// ---------------------------------------------------------------------------
// Runner — times each benchmark and prints results

inline void runAllBenchmarks() {
    // Header
    std::cout << std::left
              << std::setw(45) << "Benchmark"
              << std::right
              << std::setw(10) << "Iters"
              << std::setw(12) << "Total(ms)"
              << std::setw(12) << "Mean(us)"
              << std::setw(14) << "Throughput/s"
              << "\n";
    std::cout << std::string(93, '-') << "\n";

    for (auto& bc : benchRegistry()) {
        // Warm-up (10% of iterations, at least 1)
        size_t warmup = std::max<size_t>(1, bc.iterations / 10);
        for (size_t i = 0; i < warmup; ++i)
            bc.fn();

        auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < bc.iterations; ++i)
            bc.fn();
        auto t1 = std::chrono::steady_clock::now();

        double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double mean_us  = total_ms * 1000.0 / static_cast<double>(bc.iterations);
        double tput     = static_cast<double>(bc.iterations) / (total_ms / 1000.0);

        std::cout << std::left  << std::setw(45) << bc.name
                  << std::right
                  << std::setw(10) << bc.iterations
                  << std::setw(12) << std::fixed << std::setprecision(3) << total_ms
                  << std::setw(12) << std::fixed << std::setprecision(3) << mean_us
                  << std::setw(14) << std::fixed << std::setprecision(0) << tput
                  << "\n";
    }
}
