#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct BenchRecord {
    std::size_t matrix_size;
    std::size_t threads;
    double init_seconds;
    double multiply_seconds;
    double total_seconds;
    double speedup;
};

std::vector<std::size_t> parse_list(const std::string &raw) {
    std::vector<std::size_t> values;
    std::size_t start = 0;

    while (start < raw.size()) {
        std::size_t end = raw.find(',', start);
        if (end == std::string::npos) {
            end = raw.size();
        }
        const std::string token = raw.substr(start, end - start);
        if (!token.empty()) {
            values.push_back(static_cast<std::size_t>(std::stoull(token)));
        }
        start = end + 1;
    }

    return values;
}

template <typename F>
void parallel_for(std::size_t items, std::size_t thread_count, F fn) {
    if (items == 0) {
        return;
    }

    thread_count = std::max<std::size_t>(1, std::min(thread_count, items));
    const std::size_t chunk = (items + thread_count - 1) / thread_count;
    std::vector<std::thread> threads;
    threads.reserve(thread_count > 0 ? thread_count - 1 : 0);

    for (std::size_t t = 1; t < thread_count; ++t) {
        const std::size_t begin = t * chunk;
        const std::size_t end = std::min(items, begin + chunk);
        threads.emplace_back([=, &fn]() {
            if (begin < end) {
                fn(begin, end);
            }
        });
    }

    const std::size_t begin = 0;
    const std::size_t end = std::min(items, chunk);
    if (begin < end) {
        fn(begin, end);
    }

    for (auto &th : threads) {
        th.join();
    }
}

BenchRecord run_case(std::size_t n, std::size_t thread_count, double baseline_total) {
    const std::size_t matrix_items = n * n;
    std::vector<double> matrix(matrix_items);
    std::vector<double> vec(n);
    std::vector<double> result(n, 0.0);

    const auto init_start = std::chrono::steady_clock::now();

    parallel_for(matrix_items, thread_count, [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            matrix[i] = 1.0;
        }
    });

    parallel_for(n, thread_count, [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            vec[i] = 2.0;
        }
    });

    const auto init_end = std::chrono::steady_clock::now();

    const auto mul_start = std::chrono::steady_clock::now();

    parallel_for(n, thread_count, [&](std::size_t begin, std::size_t end) {
        for (std::size_t row = begin; row < end; ++row) {
            double sum = 0.0;
            const std::size_t base = row * n;
            for (std::size_t col = 0; col < n; ++col) {
                sum += matrix[base + col] * vec[col];
            }
            result[row] = sum;
        }
    });

    const auto mul_end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> init_dur = init_end - init_start;
    const std::chrono::duration<double> mul_dur = mul_end - mul_start;
    const double total = init_dur.count() + mul_dur.count();
    const double speedup = (baseline_total > 0.0) ? (baseline_total / total) : 1.0;

    return BenchRecord{n, thread_count, init_dur.count(), mul_dur.count(), total, speedup};
}

} // namespace

int main(int argc, char **argv) {
    std::vector<std::size_t> sizes{20000, 40000};
    std::vector<std::size_t> threads{1, 2, 4, 7, 8, 16, 20, 40};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--quick") {
            sizes = {2000, 4000};
        } else if (arg.rfind("--sizes=", 0) == 0) {
            sizes = parse_list(arg.substr(8));
        } else if (arg.rfind("--threads=", 0) == 0) {
            threads = parse_list(arg.substr(10));
        }
    }

    if (sizes.empty() || threads.empty()) {
        return 1;
    }

    for (const std::size_t n : sizes) {
        std::cout << "\nMatrix size: " << n << " x " << n << '\n';
        std::unordered_map<std::size_t, double> total_by_thread;

        for (const std::size_t tc : threads) {
            const double baseline = total_by_thread.count(1) ? total_by_thread.at(1) : 0.0;
            const BenchRecord rec = run_case(n, tc, baseline);
            total_by_thread[tc] = rec.total_seconds;

            const double speedup = total_by_thread.count(1) ? total_by_thread[1] / rec.total_seconds : 1.0;

            std::cout << "threads=" << tc << " init=" << rec.init_seconds << "s"
                      << " mult=" << rec.multiply_seconds << "s"
                      << " total=" << rec.total_seconds << "s"
                      << " speedup=" << speedup << '\n';
        }
    }

    return 0;
}
