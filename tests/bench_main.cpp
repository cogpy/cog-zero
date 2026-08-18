/*
 * standalone/tests/bench_main.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Entry point for the cog0 benchmark suite.
 * Benchmarks are registered via BENCHMARK() macros defined in bench_runner.h
 * and run sequentially.  Results are printed to stdout as a plain-text table.
 *
 * Exit code: 0 (benchmarks never "fail").
 */
#include "bench_runner.h"

#include <iostream>

// Canonical definition — all other TUs call registerBench() which calls this.
std::vector<BenchCase>& benchRegistry() {
    static std::vector<BenchCase> r;
    return r;
}

int main(int /*argc*/, char** /*argv*/) {
    std::cout << "=== cog0 Benchmark Suite ===\n\n";
    runAllBenchmarks();
    std::cout << "\nBenchmarks complete.\n";
    return 0;
}
