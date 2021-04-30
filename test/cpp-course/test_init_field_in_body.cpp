#include <algorithm>
#include <array>
#include <iterator>
#include <random>
#include <vector>

struct TestStruct
{
    TestStruct(const int * arr, size_t size)
    {
        data = std::vector<int>(arr, arr + size); // expected-warning {{'data' can be constructed in initializer list}}
        this->size = size; // expected-warning {{'size' can be constructed in initializer list}}
        three = 3; // expected-warning {{'three' can be constructed in initializer list}}
    }

    TestStruct(const std::vector<int> & vec)
    {
        data = vec; // expected-warning {{'data' can be constructed in initializer list}}
        size = vec.size(); // expected-warning {{'size' can be constructed in initializer list}}
        three = 4 - 1; // expected-warning {{'three' can be constructed in initializer list}}
    }

    template <size_t Len>
    TestStruct(const std::array<int, Len> & arr)
    {
        // code lower is just to demostrate possibility of some complex logic to initialize
        int max = std::max(arr.begin(), arr.end());
        data = std::vector<int>(max);
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> dist(-max, max);
        std::generate_n(std::back_inserter(data), max, [&dist, &gen] { return dist(gen); });
        three = 4 * 6 / 8; // expected-warning {{'three' can be constructed in initializer list}}
        size = std::count_if(arr.begin(), arr.end(), [max, this](int el) { return max - el <= three; });
    }

    std::vector<int> data;
    size_t size;
    int three;
};