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

	#pragma acc parallel loop copyout(grid[0:size], next[0:size])
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

	int n = 128;
	int nx = 0;
	int ny = 0;
	int max_iter = 1000000;
	int check_interval = 10;
	double eps = 1e-6;
	std::string out_file = "result.dat";

	po::options_description desc("Опции");
	desc.add_options()
		("help,h", "Помощь")
		("n", po::value<int>(&n)->default_value(128), "Размер сетки")
		("nx", po::value<int>(&nx), "Размер сетки по X")
		("ny", po::value<int>(&ny), "Размер сетки по Y")
		("eps,e", po::value<double>(&eps)->default_value(1e-6), "Точность")
		("max-iter,i", po::value<int>(&max_iter)->default_value(1000000), "Максимум итераций")
		("check-iter", po::value<int>(&check_interval)->default_value(10), "Проверка ошибки каждые N итераций")
		("out,o", po::value<std::string>(&out_file)->default_value("result.dat"), "Выходной результат");

	po::variables_map vm;
	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);
	} catch (const std::exception& ex) {
		std::cerr << "Error: " << ex.what() << '\n';
		std::cerr << desc << '\n';
		return 1;
	}

	if (vm.count("help") > 0) {
		std::cout << desc << '\n';
		return 0;
	}

	if (nx <= 0) {
		nx = n;
	}
	if (ny <= 0) {
		ny = n;
	}


	const size_t size = static_cast<size_t>(nx) * static_cast<size_t>(ny);
	std::unique_ptr<double[]> grid(new double[size]);
	std::unique_ptr<double[]> next(new double[size]);

	init_grids(grid.get(), next.get(), nx, ny);

	double* cur = grid.get();
	double* nxt = next.get();

	int iter = 0;
	double err = 0.0;

	const auto start_time = std::chrono::steady_clock::now();

#pragma acc data copy(cur[0:size], nxt[0:size])
	{
		while (iter < max_iter) {
			const bool do_check = (iter % check_interval == 0) || (iter == max_iter - 1);
			if (do_check) {
				err = 0.0;
#pragma acc parallel loop collapse(2) reduction(max : err)
				for (int i = 1; i < ny - 1; ++i) {
					for (int j = 1; j < nx - 1; ++j) {
						const size_t k = static_cast<size_t>(i) * static_cast<size_t>(nx) +
									 static_cast<size_t>(j);
						const double new_val = 0.25 * (cur[k - 1] + cur[k + 1] +
									   cur[k - static_cast<size_t>(nx)] +
									   cur[k + static_cast<size_t>(nx)]);
						nxt[k] = new_val;
						const double diff = std::fabs(new_val - cur[k]);
						if (diff > err) {
							err = diff;
						}
					}
				}
			} else {
#pragma acc parallel loop collapse(2)
				for (int i = 1; i < ny - 1; ++i) {
					for (int j = 1; j < nx - 1; ++j) {
						const size_t k = static_cast<size_t>(i) * static_cast<size_t>(nx) +
									 static_cast<size_t>(j);
						const double new_val = 0.25 * (cur[k - 1] + cur[k + 1] +
									   cur[k - static_cast<size_t>(nx)] +
									   cur[k + static_cast<size_t>(nx)]);
						nxt[k] = new_val;
					}
				}
			}

			std::swap(cur, nxt);
			++iter;

			if (do_check && err <= eps) {
				break;
			}
		}
	}

	const auto end_time = std::chrono::steady_clock::now();
	const std::chrono::duration<double> elapsed = end_time - start_time;

	std::cout << "Итераций: " << iter << ", Ошибка: " << err
			  << ", Время: " << elapsed.count() << " сек" << '\n';

	try {
		save_matrix(out_file, cur, nx, ny);
	} catch (const std::exception& ex) {
		std::cerr << ex.what() << '\n';
		return 1;
	}

	if (nx == ny && (nx == 10 || nx == 13)) {
		print_matrix(cur, nx, ny);
	}

	return 0;
}
