#include <boost/program_options.hpp>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void set_boundaries(double* grid, double* next, int nx, int ny) {
  const double tl = 10.0;
  const double tr = 20.0;
  const double br = 30.0;
  const double bl = 20.0;

  #pragma acc parallel loop
  for (int j = 0; j < nx; ++j) {
    const double t = static_cast<double>(j) / static_cast<double>(nx - 1);
    const double top = tl + t * (tr - tl);
    const double bottom = bl + t * (br - bl);
    const size_t top_idx = static_cast<size_t>(j);
    const size_t bottom_idx = static_cast<size_t>(ny - 1) *
                static_cast<size_t>(nx) + static_cast<size_t>(j);
    grid[top_idx] = top;
    next[top_idx] = top;
    grid[bottom_idx] = bottom;
    next[bottom_idx] = bottom;
  }

  #pragma acc parallel loop
  for (int i = 0; i < ny; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(ny - 1);
    const double left = tl + t * (bl - tl);
    const double right = tr + t * (br - tr);
    const size_t left_idx = static_cast<size_t>(i) * static_cast<size_t>(nx);
    const size_t right_idx = left_idx + static_cast<size_t>(nx - 1);
    grid[left_idx] = left;
    next[left_idx] = left;
    grid[right_idx] = right;
    next[right_idx] = right;
  }
}

void init_grids(double* grid, double* next, int nx, int ny) {
  const int size = nx * ny;

  #pragma acc parallel loop
  for (int k = 0; k < size; ++k) {
    grid[k] = 0.0;
    next[k] = 0.0;
  }

  set_boundaries(grid, next, nx, ny);
}

void save_matrix(const std::string& path, const double* grid, int nx, int ny) {
  std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("Ошибка при открытии файла: " + path);
  }

  out << std::setprecision(12);
  for (int i = 0; i < ny; ++i) {
    const size_t row = static_cast<size_t>(i) * static_cast<size_t>(nx);
    for (int j = 0; j < nx; ++j) {
      if (j > 0) {
        out << ' ';
      }
      out << grid[row + static_cast<size_t>(j)];
    }
    out << '\n';
  }
}

void print_matrix(const double* grid, int nx, int ny) {
  std::cout << std::fixed << std::setprecision(6);
  for (int i = 0; i < ny; ++i) {
    const size_t row = static_cast<size_t>(i) * static_cast<size_t>(nx);
    for (int j = 0; j < nx; ++j) {
      std::cout << grid[row + static_cast<size_t>(j)];
      if (j + 1 < nx) {
        std::cout << ' ';
      }
    }
    std::cout << '\n';
  }
}

}  // namespace

int main(int argc, char** argv) {
  namespace po = boost::program_options;
  int n = 128, nx = 0, ny = 0, max_iter = 1000000;
  double eps = 1e-6;

  std::string out_file = "result.dat";

  po::options_description desc("Опции");
  desc.add_options()("n", po::value<int>(&n)->default_value(128))
                   ("nx", po::value<int>(&nx))("ny", po::value<int>(&ny))
                   ("eps,e", po::value<double>(&eps)->default_value(1e-6))
                   ("max-iter,i", po::value<int>(&max_iter)->default_value(1000000))
                   ("out,o", po::value<std::string>(&out_file)->default_value("result.dat"));
  
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm); po::notify(vm);

  nx = (nx <= 0) ? n : nx; ny = (ny <= 0) ? n : ny;
  const size_t size = (size_t)nx * ny;
  std::unique_ptr<double[]> grid(new double[size]), next(new double[size]);
  init_grids(grid.get(), next.get(), nx, ny);

  double *cur = grid.get(), *nxt = next.get();
  int iter = 0;
  double err = 0.0;

  const auto start_time = std::chrono::steady_clock::now();

  while (iter < max_iter) {
    err = 0.0;
#pragma acc parallel loop collapse(2) reduction(max : err)
    for (int i = 1; i < ny - 1; ++i) {
      for (int j = 1; j < nx - 1; ++j) {
        size_t k = (size_t)i * nx + j;
        
        double val = 0.25 * (cur[k - 1] + cur[k + 1] + cur[k - nx] + cur[k + nx]);
        nxt[k] = val;
        
        // Вычисление относительной ошибки с защитой от деления на ноль
        double rel_diff = (std::fabs(val) > 1e-12) ? std::fabs(val - cur[k]) / std::fabs(val) : std::fabs(val - cur[k]);

        if (rel_diff > err) {
          err = rel_diff;
        }
      }
    }

    std::swap(cur, nxt);
    ++iter;

    if (err <= eps) {
      break;
    }
  }

  const auto end_time = std::chrono::steady_clock::now();
  const std::chrono::duration<double> elapsed = end_time - start_time;

  std::cout << "Итераций: " << iter << ", Относительная ошибка: " << err
        << ", Время: " << elapsed.count() << " сек" << '\n';

  save_matrix(out_file, cur, nx, ny);

  if (nx == ny && (nx == 10 || nx == 13)) {
    std::cout << "\nМатрица результатов (" << nx << "x" << ny << "):\n";
    print_matrix(cur, nx, ny);
  }

  return 0;
}