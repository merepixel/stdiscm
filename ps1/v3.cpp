#include "helpers.hpp"

int main() {
    const auto cfg = load_config();

    print_line("[RUN START] " + now_timestamp());
    auto t0 = std::chrono::steady_clock::now();

    // std::size_t primes_found = 0;
    std::atomic<std::size_t> primes_found{0};


    auto print_prime = [&](std::uint64_t n, std::thread::id tid){
        std::ostringstream oss;
        oss << "[" << now_timestamp() << "] [thread " << tid << "] prime=" << n;
        print_line(oss.str());
    };

    for (std::uint64_t n = 2; n <= cfg.limit; ++n) {
        if (n == 2 || n == 3) { ++primes_found; print_prime(n, std::this_thread::get_id()); continue; }
        if ((n % 2) == 0) continue; 
        std::uint64_t s = static_cast<std::uint64_t>(std::sqrt((long double)n));
        if (s < 3) { ++primes_found; print_prime(n, std::this_thread::get_id()); continue; }
        std::uint64_t odd_cnt = (s >= 3) ? ((s - 3) / 2 + 1) : 0;
        unsigned int k = (odd_cnt == 0) ? 0 : static_cast<unsigned int>(std::min<std::uint64_t>(cfg.threads, odd_cnt));
        if (k == 0) { ++primes_found; print_prime(n, std::this_thread::get_id()); continue; }

        std::atomic<bool> composite{false};
        std::atomic<unsigned> remaining{k};
        std::vector<std::thread> testers;
        testers.reserve(k);

        auto tester = [&](unsigned idx){
            for (std::uint64_t d = 3 + 2*idx; d <= s && !composite.load(std::memory_order_relaxed); d += 2*k) {
                if ((n % d) == 0) { composite.store(true, std::memory_order_relaxed); break; }
            }
            if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                if (!composite.load(std::memory_order_acquire)) {
                    ++primes_found;
                    print_prime(n, std::this_thread::get_id());
                }
            }
        };
        for (unsigned i = 0; i < k; ++i) testers.emplace_back(tester, i); 
        for (auto& th : testers) th.join();
    }
    auto t1 = std::chrono::steady_clock::now();
    print_line ("[RUN END] " + now_timestamp ());
    print_summary("Variant 3", cfg, t1 - t0, primes_found);
    return 0;
}