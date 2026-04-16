#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Row {
    std::size_t id;
    std::string op;
    double a;
    double b;
    double result;
};

Row parse_row(const std::string &line) {
    std::stringstream ss(line);
    std::string part;
    Row row{};

    std::getline(ss, part, ',');
    row.id = static_cast<std::size_t>(std::stoull(part));

    std::getline(ss, row.op, ',');

    std::getline(ss, part, ',');
    row.a = std::stod(part);

    std::getline(ss, part, ',');
    row.b = std::stod(part);

    std::getline(ss, part);
    row.result = std::stod(part);

    return row;
}

bool nearly_equal(double lhs, double rhs, double abs_eps = 1e-10, double rel_eps = 1e-8) {
    const double diff = std::abs(lhs - rhs);
    if (diff <= abs_eps) {
        return true;
    }
    return diff <= rel_eps * std::max(std::abs(lhs), std::abs(rhs));
}

void validate_file(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    std::size_t line_no = 0;
    std::string line;
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) {
            continue;
        }

        const Row row = parse_row(line);
        double expected = 0.0;
        if (row.op == "sin") {
            expected = std::sin(row.a);
        } else if (row.op == "sqrt") {
            expected = std::sqrt(row.a);
        } else if (row.op == "pow") {
            expected = std::pow(row.a, row.b);
        } else {
            throw std::runtime_error("Unknown operation in " + path + " line " + std::to_string(line_no));
        }

        if (!nearly_equal(expected, row.result)) {
            std::ostringstream err;
            err << "Mismatch in " << path << " line " << line_no << ": expected " << expected << " got "
                << row.result;
            throw std::runtime_error(err.str());
        }
    }
}

} // namespace

int main() {
    try {
        validate_file("results/client_sin.txt");
        validate_file("results/client_sqrt.txt");
        validate_file("results/client_pow.txt");
        std::cout << "All result files are valid.\n";
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "Test failed: " << ex.what() << '\n';
        return 1;
    }
}
