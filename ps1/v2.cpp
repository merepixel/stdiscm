#include "helpers.hpp"

int main() {
    const auto cfg = load_config();
    print_line("[RUN START] " + now_timestamp());
    auto t0 = std::chrono::steady_clock::now();

    std::vector<std::thread> workers;
    std::uint64_t start = 2;
    std::uint64_t end = cfg.limit;
    std::uint64_t total = (end >= start) ? (end - start + 1) : 0;
    std::vector<std::vector<std::uint64_t>> buckets(cfg.threads);

    auto worker = [&](unsigned idx){
        std::uint64_t a = start + (total * idx) / cfg.threads;
        std::uint64_t b = start + (total * (idx + 1)) / cfg.threads;
        if (b > end + 1) b = end + 1; 
        if (a >= b) return;
        auto& out = buckets[idx];
        out.reserve((b - a) / 10 + 1);
        for (std::uint64_t n = a; n < b; ++n) if (is_prime_single(n)) out.push_back(n);
    };

    for (unsigned i = 0; i < cfg.threads; ++i) workers.emplace_back(worker, i);
    for (auto& th : workers) th.join();
    std::vector<std::uint64_t> primes;
    std::size_t total_sz = 0; for (auto& v : buckets) total_sz += v.size();
    primes.reserve(total_sz);
    for (auto& v : buckets) primes.insert(primes.end(), v.begin(), v.end());
    std::sort(primes.begin(), primes.end());

    for (auto p : primes) std::cout << p << '\n';
    auto t1 = std::chrono::steady_clock::now();
    print_line("[RUN END] " + now_timestamp());
    print_summary("Variant 2", cfg, t1 - t0, primes.size());
    return 0;
}