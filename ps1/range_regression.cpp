#include "helpers.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

namespace {

void check_ranges(std::uint64_t limit, unsigned int threads) {
    const std::uint64_t start = 2;
    assert(limit >= start);
    const std::uint64_t total = limit - start + 1;

    std::uint64_t sum = 0;
    std::uint64_t last_begin = start;
    bool saw_non_empty = false;

    for (unsigned int idx = 0; idx < threads; ++idx) {
        WorkerSlice slice = compute_worker_slice(start, total, threads, idx);
        if (slice.count == 0) {
            if (sum >= total) {
                assert(slice.begin == start);
            }
            continue;
        }

        assert(slice.begin >= last_begin);
        assert(slice.begin == start + sum);
        assert(slice.begin >= start);
        assert(slice.begin <= limit);
        assert(slice.count <= total - sum);

        std::uint64_t last_value = slice.begin + (slice.count - 1);
        assert(last_value <= limit);

        sum += slice.count;
        last_begin = slice.begin;
        saw_non_empty = true;
    }

    assert(saw_non_empty);
    assert(sum == total);
}

}

int main() {
    check_ranges(std::numeric_limits<std::uint64_t>::max() - 16, 1'000'000);
    check_ranges(std::numeric_limits<std::uint64_t>::max() - 16, 64);
    check_ranges(1000, 10);
    check_ranges(1000, 1500);
    check_ranges(10, 32);
    return 0;
}
