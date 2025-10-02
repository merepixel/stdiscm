#ifndef helpers_hpp
#define helpers_hpp

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>


struct WorkerSlice {
    std::uint64_t begin;
    std::uint64_t count;
};

inline WorkerSlice compute_worker_slice(std::uint64_t start,
                                        std::uint64_t total,
                                        unsigned int threads,
                                        unsigned int idx) {
    WorkerSlice slice{start, 0};
    if (threads == 0 || total == 0 || idx >= threads) {
        return slice;
    }

    std::uint64_t chunk = total / threads;
    std::uint64_t rem = total % threads;
    std::uint64_t idx64 = static_cast<std::uint64_t>(idx);
    std::uint64_t base_offset = static_cast<std::uint64_t>(
        static_cast<__uint128_t>(idx64) * chunk +
        std::min<std::uint64_t>(idx64, rem));
    std::uint64_t next_idx64 = idx64 + 1;
    std::uint64_t next_offset = static_cast<std::uint64_t>(
        static_cast<__uint128_t>(next_idx64) * chunk +
        std::min<std::uint64_t>(next_idx64, rem));

    slice.count = next_offset - base_offset;
    if (slice.count == 0) {
        return slice;
    }

    slice.begin = start + base_offset;
    return slice;
}


struct Config {
    unsigned int threads = std::max(1u, std::thread::hardware_concurrency());
    std::uint64_t limit = 100000;
};

inline std::mutex& cout_mutex() {
    static std::mutex m;
    return m;
}

inline std::string now_timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto tt = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

inline void print_line(const std::string& s) {
    std::lock_guard<std::mutex> lock(cout_mutex());
    std::cout << s << '\n';
}

inline std::optional<Config> read_config_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) return std::nullopt;
    Config cfg;
    std::string line;
    while (std::getline(in, line)) {
        auto trim = [](std::string& x){
            while(!x.empty() && (x.front()==' ' || x.front()=='\t')) x.erase(x.begin());
            while(!x.empty() && (x.back()==' ' || x.back()=='\t' || x.back()=='\r')) x.pop_back();
        };
        trim(line);
        if (line.empty() || line[0]=='#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0,pos);
        std::string val = line.substr(pos+1);
        trim(key); trim(val);
        if (key == "threads") {
            try {
                long long t = std::stoll(val);
                if (t >= 1 && t <= 1'000'000) cfg.threads = static_cast<unsigned int>(t);
            } catch (...) {}
        } else if (key == "limit") {
            try {
                long double ld = std::stold(val);
                if (ld >= 2 && ld <= 9.22e18L) cfg.limit = static_cast<std::uint64_t>(ld);
            } catch (...) {}
        }
    }
    return cfg;
}

inline Config load_config(const std::string& path = "config.txt") {
    Config cfg;
    auto c = read_config_file(path);
    if (c) cfg = *c; else print_line("[WARNING] config.txt not found — using defaults.");
    if (cfg.threads == 0) { cfg.threads = 1; print_line("[WARNING] threads < 1 — clamped to 1."); }
    if (cfg.limit < 2) { cfg.limit = 2; print_line("[WARNING] limit < 2 — clamped to 2."); }
    return cfg;
}

inline bool is_prime_single(std::uint64_t n) {
    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if ((n % 2) == 0) return false;
    std::uint64_t s = static_cast<std::uint64_t>(std::sqrt((long double)n));
    for (std::uint64_t d = 3; d <= s; d += 2) {
        if ((n % d) == 0) return false;
    }
    return true;
}

inline void print_summary(const char* title,
                          const Config& cfg,
                          std::chrono::steady_clock::duration elapsed,
                          std::size_t primes_found) {
    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(elapsed).count();
    std::ostringstream oss;
    oss << "[SUMMARY] " << title
        << " | threads=" << cfg.threads
        << " | limit=" << cfg.limit
        << " | primes=" << primes_found
        << " | elapsed=" << ms << " ms";
    print_line(oss.str());
}

#endif 