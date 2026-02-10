#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>

using namespace std;

int main() {
    const size_t N = 10000000;

#ifdef USE_DOUBLE
    using arrType = double;
#else
    using arrType = float;
#endif

    vector<arrType> arr(N);

    for (size_t i = 0; i < N; ++i) {
        arr[i] = sin(2.0 * static_cast<arrType>(M_PI * i) / static_cast<arrType>(N));
    }

    arrType sum = 0.0;
    for (const auto& val : arr) {
        sum += val;
    }


    cout << "Сумма элементов массива: " << fixed << setprecision(25) << sum << endl;

    return 0;
}