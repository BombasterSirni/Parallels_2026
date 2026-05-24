#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char** argv) {
    int grid_size = 128;
    int max_iter = 1000000;
    double tol = 1e-6;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("accuracy", po::value<double>(&tol)->default_value(1e-6), "precision limit")
        ("size", po::value<int>(&grid_size)->default_value(128), "grid size (e.g. 128 for 128x128)")
        ("iter", po::value<int>(&max_iter)->default_value(1000000), "maximum number of iterations")
    ;

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    int N = grid_size;
    
    std::vector<double> grid(N * N, 0.0);
    std::vector<double> new_grid(N * N, 0.0);

    double top_left = 10.0, top_right = 20.0;
    double bottom_left = 20.0, bottom_right = 30.0;

    for (int i = 0; i < N; ++i) {
        // top edge (row 0)
        grid[0 * N + i] = top_left + i * (top_right - top_left) / (N - 1);
        // bottom edge (row N-1)
        grid[(N - 1) * N + i] = bottom_left + i * (bottom_right - bottom_left) / (N - 1);
        // left edge (col 0)
        grid[i * N + 0] = top_left + i * (bottom_left - top_left) / (N - 1);
        // right edge (col N-1)
        grid[i * N + (N - 1)] = top_right + i * (bottom_right - top_right) / (N - 1);
    }

    for (int i = 0; i < N; ++i) {
        new_grid[0 * N + i] = grid[0 * N + i];
        new_grid[(N - 1) * N + i] = grid[(N - 1) * N + i];
        new_grid[i * N + 0] = grid[i * N + 0];
        new_grid[i * N + (N - 1)] = grid[i * N + (N - 1)];
    }

    double* A = grid.data();
    double* Anew = new_grid.data();

    double error = 1.0;
    int iter = 0;

    #pragma acc data copy(A[0:N*N], Anew[0:N*N])
    {
        while (error > tol && iter < max_iter) {
            error = 0.0;

            #pragma acc parallel loop collapse(2) present(A, Anew)
            for (int i = 1; i < N - 1; ++i) {
                for (int j = 1; j < N - 1; ++j) {
                    Anew[i * N + j] = 0.25 * (A[(i - 1) * N + j] + A[(i + 1) * N + j] + A[i * N + (j - 1)] + A[i * N + (j + 1)]);
                }
            }

            #pragma acc parallel loop collapse(2) reduction(max:error) present(A, Anew)
            for (int i = 1; i < N - 1; ++i) {
                for (int j = 1; j < N - 1; ++j) {
                    double diff = std::abs(Anew[i * N + j] - A[i * N + j]);
                    if (diff > error) {
                        error = diff;
                    }
                }
            }

            // Using pointer swapping to avoid full array copy, but we need to ensure OpenACC handles it.
            // A safer approach inside the data region is to just copy inside a parallel loop if pointer swap fails on device.
            #pragma acc parallel loop collapse(2) present(A, Anew)
            for (int i = 1; i < N - 1; ++i) {
                for (int j = 1; j < N - 1; ++j) {
                    A[i * N + j] = Anew[i * N + j];
                }
            }
            
            iter++;
        }
    }

    std::cout << "Iterations: " << iter << "\n";
    std::cout << "Error: " << std::scientific << std::setprecision(6) << error << "\n";

    if (N <= 13) {
        std::cout << "\nGrid output:\n";
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                std::cout << std::fixed << std::setprecision(2) << std::setw(6) << A[i * N + j] << " ";
            }
            std::cout << "\n";
        }
    }

    return 0;
}
