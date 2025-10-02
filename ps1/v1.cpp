#include "helpers.hpp"
#include <future>

int main() {
    const auto cfg = load_config();
    print_line("[RUN START] " + now_timestamp());
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> workers;
    std::uint64_t start = 2;
    std::uint64_t end = cfg.limit;

    if (end < start) {
        print_line("[ERROR] limit < 2");
        return 1;
    }

    std::uint64_t total = end - start + 1;
    auto worker = [&](unsigned idx){
        auto slice = compute_worker_slice(start, total, cfg.threads, idx);
        if (slice.count == 0) return;
        for (std::uint64_t offset = 0; offset < slice.count; ++offset) {
            std::uint64_t n = slice.begin + offset;
            if (is_prime_single(n)) {
                std::ostringstream oss;
                oss << "[" << now_timestamp() << "] [thread " << std::this_thread::get_id()
                    << "] prime=" << n;
                print_line(oss.str());
            }
        }
    };

    for (unsigned i = 0; i < cfg.threads; ++i) workers.emplace_back(worker, i);
    for (auto& th : workers) th.join();
    auto t1 = std::chrono::steady_clock::now();
    print_line("[RUN END] " + now_timestamp());

    std::size_t primes = 0;
    for (std::uint64_t n = 2; n <= cfg.limit; ++n) if (is_prime_single(n)) ++primes;
    print_summary("Variant 1", cfg, t1 - t0, primes);
    return 0;
}