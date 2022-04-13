#include <array>
#include <vector>

struct TestStruct
{
    TestStruct(const int * arr, std::size_t size)
    {
        std::vector<int> to_fill; // expected-warning {{'to_fill' is not needed temporary, values can be written directly into field}}
        if (arr) {
            for (std::size_t i = 0; i < size; ++i) {
                to_fill.push_back(arr[i]);
            }
            data = std::move(to_fill);
        }
    }

    template <std::size_t Size>
    TestStruct(const std::array<int, Size> & arr)
    {
        std::vector<int> to_fill; // expected-warning {{'to_fill' is not needed temporary, values can be written directly into field}}
        for (auto i = 0; i < Size; ++i) {
            to_fill.push_back(arr[i]);
        }
        data = to_fill;
    }

    std::vector<int> data;
};

void instantiate()
{
    std::array arr = {1, 2, 3};
    TestStruct ts(arr);
}