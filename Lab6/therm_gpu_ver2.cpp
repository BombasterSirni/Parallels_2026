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
  const int size = nx * ny;
  const double tl = 10.0, tr = 20.0, br = 30.0, bl = 20.0;
  #pragma acc parallel loop copyout(grid[0:size], next[0:size])
  for (int j = 0; j < nx; ++j) {
    const double t = (double)j / (nx - 1);
    grid[j] = next[j] = tl + t * (tr - tl);
    grid[(ny - 1) * nx + j] = next[(ny - 1) * nx + j] = bl + t * (br - bl);
  }
  #pragma acc parallel loop copyout(grid[0:size], next[0:size])
  for (int i = 0; i < ny; ++i) {
    const double t = (double)i / (ny - 1);
    grid[i * nx] = next[i * nx] = tl + t * (bl - tl);
    grid[i * nx + (nx - 1)] = next[i * nx + (nx - 1)] = tr + t * (br - tr);
  }
}

void init_grids(double* grid, double* next, int nx, int ny) {
  const size_t size = (size_t)nx * ny;
  #pragma acc parallel loop
  for (size_t k = 0; k < size; ++k) { grid[k] = 0.0; next[k] = 0.0; }
  set_boundaries(grid, next, nx, ny);
}

void save_matrix(const std::string& path, const double* grid, int nx, int ny) {
  std::ofstream out(path.c_str());
  for (int i = 0; i < ny; ++i) {
    for (int j = 0; j < nx; ++j) out << grid[i * nx + j] << (j == nx - 1 ? "" : " ");
    out << '\n';
  }
}

void print_matrix(const double* grid, int nx, int ny) {
  std::cout << std::fixed << std::setprecision(4);
  for (int i = 0; i < ny; ++i) {
    for (int j = 0; j < nx; ++j) {
      std::cout << std::setw(8) << grid[i * nx + j] << " ";
    }
    std::cout << '\n';
  }
}
} // namespace

int main(int argc, char** argv) {
  namespace po = boost::program_options;
  int n = 128, nx = 0, ny = 0, max_iter = 1000000, check_iter = 100;
  double eps = 1e-6;
  std::string out_file = "result.dat";

  po::options_description desc("Опции");
  desc.add_options()("n", po::value<int>(&n)->default_value(128))
                   ("nx", po::value<int>(&nx))("ny", po::value<int>(&ny))
                   ("eps,e", po::value<double>(&eps)->default_value(1e-6))
                   ("max-iter,i", po::value<int>(&max_iter)->default_value(1000000))
                   ("check-iter,c", po::value<int>(&check_iter)->default_value(100))
                   ("out,o", po::value<std::string>(&out_file)->default_value("result.dat"));
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm); po::notify(vm);
  
  nx = (nx <= 0) ? n : nx; ny = (ny <= 0) ? n : ny;
  if (check_iter <= 0) check_iter = 1;

  const size_t size = (size_t)nx * ny;
  std::unique_ptr<double[]> grid(new double[size]), next(new double[size]);
  init_grids(grid.get(), next.get(), nx, ny);

  double *cur = grid.get(), *nxt = next.get();
  int iter = 0;
  double err = 0.0;

  const auto start_time = std::chrono::steady_clock::now();

  #pragma acc data copy(cur[0:size], nxt[0:size])
  {
    while (iter < max_iter) {
      bool need_check = ((iter + 1) % check_iter == 0);
      err = 0.0;

      if (need_check) {
        #pragma acc parallel loop collapse(2) reduction(max:err) present(cur[0:size], nxt[0:size])
        for (int i = 1; i < ny - 1; ++i) {
          for (int j = 1; j < nx - 1; ++j) {
            size_t k = (size_t)i * nx + j;
            double val = 0.25 * (cur[k - 1] + cur[k + 1] + cur[k - nx] + cur[k + nx]);
            nxt[k] = val;
            double diff = (std::fabs(val) > 1e-12) ? std::fabs(val - cur[k]) / std::fabs(val) : std::fabs(val - cur[k]);
            if (diff > err) err = diff;
          }
        }
      } else {
        #pragma acc parallel loop collapse(2) present(cur[0:size], nxt[0:size])
        for (int i = 1; i < ny - 1; ++i) {
          for (int j = 1; j < nx - 1; ++j) {
            size_t k = (size_t)i * nx + j;
            nxt[k] = 0.25 * (cur[k - 1] + cur[k + 1] + cur[k - nx] + cur[k + nx]);
          }
        }
      }

      std::swap(cur, nxt);
      iter++;
      if (need_check && err <= eps) break;
    }
  }

  const auto end_time = std::chrono::steady_clock::now();
  std::cout << "Итераций: " << iter << ", Ошибка: " << err << ", Время: " 
            << std::chrono::duration<double>(end_time - start_time).count() << " сек\n";

  save_matrix(out_file, cur, nx, ny);
  if (nx == ny && (nx == 10 || nx == 13)) {
    print_matrix(cur, nx, ny);
  }

  return 0;
}