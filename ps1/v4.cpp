#include "helpers.hpp"

int main() {
    const auto cfg = load_config();

    print_line("[RUN START] " + now_timestamp());
    auto t0 = std::chrono::steady_clock::now();

    std::vector<std::uint64_t> primes;
    // primes.reserve(50000); 
    auto estimate_capacity = [](std::uint64_t n) -> std::size_t {
        if (n < 10) return static_cast<std::size_t>(n / 2);
        long double ln = std::log(static_cast<long double>(n));
        long double estimate = (static_cast<long double>(n) / ln) * 1.2L; // 20% safety margin
        if (estimate < 1) estimate = 1;
        return static_cast<std::size_t>(estimate);
    };
    primes.reserve(estimate_capacity(cfg.limit));

    for (std::uint64_t n = 2; n <= cfg.limit; ++n) {
        if (n == 2 || n == 3) { primes.push_back(n); continue; }
        if ((n % 2) == 0) continue; 
        std::uint64_t s = static_cast<std::uint64_t>(std::sqrt((long double)n));
        if (s < 3) { primes.push_back(n); continue; }
        std::uint64_t odd_cnt = (s >= 3) ? ((s - 3) / 2 + 1) : 0;
        unsigned int k = (odd_cnt == 0)
            ? 0
            : static_cast<unsigned int>(std::min<std::uint64_t>(cfg.threads, odd_cnt));
        if (k == 0) { primes.push_back(n); continue; }

        std::atomic<bool> composite{false};
        std::atomic<unsigned> remaining{k};
        std::vector<std::thread> testers;
        testers.reserve(k);
        std::mutex push_mutex;

        auto tester = [&](unsigned idx){
            for (std::uint64_t d = 3 + 2*idx;
                 d <= s && !composite.load(std::memory_order_relaxed);
                 d += 2*k) {
                if ((n % d) == 0) {
                    composite.store(true, std::memory_order_relaxed);
                    break;
                }
            }
            if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                if (!composite.load(std::memory_order_acquire)) {
                    std::lock_guard<std::mutex> lk(push_mutex);
                    primes.push_back(n);
                }
            }
        };

        for (unsigned i = 0; i < k; ++i) testers.emplace_back(tester, i);
        for (auto& th : testers) th.join();
    }

    std::sort(primes.begin(), primes.end());
    for (auto p : primes) std::cout << p << '\n';

    auto t1 = std::chrono::steady_clock::now();
    print_line("[RUN END] " + now_timestamp());
    print_summary("Variant 4", cfg, t1 - t0, primes.size());
    return 0;
}