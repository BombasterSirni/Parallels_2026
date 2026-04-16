#include "server.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void run_sin_client(TaskServer<double> &server, int n, const std::string &file_path) {
    std::mt19937_64 gen(std::random_device{}());
    std::uniform_real_distribution<double> dist(-1000.0, 1000.0);

    std::ofstream out(file_path);
    if (!out) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }

    out << std::setprecision(17);
    for (int i = 0; i < n; ++i) {
        const double x = dist(gen);
        const std::size_t id = server.add_task([x]() { return std::sin(x); });
        const double result = server.request_result(id);
        out << id << ",sin," << x << ",0," << result << '\n';
    }
}

void run_sqrt_client(TaskServer<double> &server, int n, const std::string &file_path) {
    std::mt19937_64 gen(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, 1'000'000.0);

    std::ofstream out(file_path);
    if (!out) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }

    out << std::setprecision(17);
    for (int i = 0; i < n; ++i) {
        const double x = dist(gen);
        const std::size_t id = server.add_task([x]() { return std::sqrt(x); });
        const double result = server.request_result(id);
        out << id << ",sqrt," << x << ",0," << result << '\n';
    }
}

void run_pow_client(TaskServer<double> &server, int n, const std::string &file_path) {
    std::mt19937_64 gen(std::random_device{}());
    std::uniform_real_distribution<double> base_dist(0.1, 10.0);
    std::uniform_real_distribution<double> exp_dist(0.0, 5.0);

    std::ofstream out(file_path);
    if (!out) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }

    out << std::setprecision(17);
    for (int i = 0; i < n; ++i) {
        const double x = base_dist(gen);
        const double y = exp_dist(gen);
        const std::size_t id = server.add_task([x, y]() { return std::pow(x, y); });
        const double result = server.request_result(id);
        out << id << ",pow," << x << ',' << y << ',' << result << '\n';
    }
}

} // namespace

int main(int argc, char **argv) {
    int n = 100;
    if (argc > 1) {
        n = std::stoi(argv[1]);
    }
    if (n <= 5 || n >= 10000) {
        std::cerr << "5 < N < 10000\n";
        return 1;
    }

    try {
        std::filesystem::create_directories("results");

        TaskServer<double> server;
        server.start();

        std::thread c1(run_sin_client, std::ref(server), n, "results/client_sin.txt");
        std::thread c2(run_sqrt_client, std::ref(server), n, "results/client_sqrt.txt");
        std::thread c3(run_pow_client, std::ref(server), n, "results/client_pow.txt");

        c1.join();
        c2.join();
        c3.join();

        server.stop();

        std::cout << "Files created:\n"
                  << "  results/client_sin.txt\n"
                  << "  results/client_sqrt.txt\n"
                  << "  results/client_pow.txt\n";
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
