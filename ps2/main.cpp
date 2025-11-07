#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <charconv>

using namespace std;

// trim/parsing helpers 
static inline string_view trim_left(string_view s) noexcept {
    size_t i = 0; while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) ++i; return s.substr(i);
}
static inline string_view trim_right(string_view s) noexcept {
    size_t i = s.size(); while (i > 0 && isspace(static_cast<unsigned char>(s[i-1]))) --i; return s.substr(0, i);
}
static inline string_view trim(string_view s) noexcept { return trim_right(trim_left(s)); }
static inline string_view strip_comment(string_view s) noexcept {
    size_t pos = s.find('#'); return (pos == string_view::npos) ? s : s.substr(0, pos);
}
template <class Int>
static bool parse_integral_sv(string_view sv, Int& out) noexcept {
    sv = trim(sv); if (sv.empty()) return false;
    const char* first = sv.data(); const char* last = sv.data() + sv.size();
    Int tmp{}; auto [ptr, ec] = std::from_chars(first, last, tmp);
    if (ec != std::errc{} || ptr != last) return false; out = tmp; return true;
}
template <class Int, class Validator>
static bool read_one_value(string_view prompt, string_view expected_key,
                           Int& out, string_view extra_rule_msg, Validator validator) {
    for (;;) {
        cout << prompt << flush;
        string line;
        if (!getline(cin, line)) { cerr << "\nError: unexpected end of input while reading " << expected_key << ".\n"; return false; }
        string_view sv = trim(strip_comment(line));
        if (sv.empty()) continue;

        string_view val_part;
        size_t eq = sv.find('=');
        if (eq == string_view::npos) {
            val_part = sv; // raw value
        } else {
            string_view key = trim(sv.substr(0, eq));
            if (key != expected_key) {
                cerr << "Invalid line: expected key '" << expected_key << "' before '=', got '" << string(key) << "'.\n";
                continue;
            }
            val_part = trim(sv.substr(eq + 1));
        }
        Int candidate{};
        if (!parse_integral_sv(val_part, candidate)) {
            cerr << "Invalid numeric format for " << expected_key << ". Please enter an integer";
            if (!extra_rule_msg.empty()) cerr << " (" << extra_rule_msg << ")";
            cerr << ".\n"; continue;
        }
        if (!validator(candidate)) {
            cerr << "Invalid value for " << expected_key;
            if (!extra_rule_msg.empty()) cerr << " (" << extra_rule_msg << ")";
            cerr << ".\n"; continue;
        }
        out = candidate; return true;
    }
}

// time helper
static string now_hhmmss() {
    using clock = chrono::system_clock;
    auto tp = clock::now();
    time_t t = clock::to_time_t(tp);
    tm bt{};
#if defined(_WIN32)
    localtime_s(&bt, &t);
#else
    localtime_r(&t, &bt);
#endif
    char buf[16]; strftime(buf, sizeof(buf), "%H:%M:%S", &bt); return string(buf);
}

// shared state (guarded by m unless stated)
struct Shared {
    mutex m; condition_variable cv;
    mutex out_m;

    size_t n{};
    vector<bool> active;         // instance running
    vector<bool> has_job;        // assignment waiting for worker pickup
    vector<int>  job_secs;       // assigned duration
    vector<size_t> served;       // parties served per instance
    vector<uint64_t> total_secs; // total time per instance
    deque<size_t> idle;          // FIFO idle instances

    size_t total_parties{};      // fixed workload
    size_t scheduled_parties{};
    bool shutdown{false};

    // for final summary
    size_t in_tanks{}, in_healers{}, in_dps{};
    size_t unmatched_tanks{}, unmatched_healers{}, unmatched_dps{}, unmatched_total{};

    size_t total_served_nolock() const noexcept {
        return accumulate(served.begin(), served.end(), size_t{0});
    }
    uint64_t grand_total_secs_nolock() const noexcept {
        return accumulate(total_secs.begin(), total_secs.end(), uint64_t{0});
    }

    void print_status_snapshot(string_view header) {
        lock_guard<mutex> lg(out_m);
        cout << "[" << now_hhmmss() << "] " << header << "\n";
        if (n == 0) { cout << "No instances available.\n"; cout.flush(); return; }
        for (size_t i = 0; i < n; ++i) {
            bool a; { lock_guard<mutex> lk(m); a = active[i]; }
            cout << "  Instance " << i << ": " << (a ? "active" : "empty") << "\n";
        }
        cout.flush();
    }

    void print_final_summary() {
        lock_guard<mutex> lg(out_m);
        cout << "\n****** Summary ******\n";
        if (n == 0) { cout << "No instances existed.\n"; }
        for (size_t i = 0; i < n; ++i) {
            size_t s; uint64_t secs; { lock_guard<mutex> lk(m); s = served[i]; secs = total_secs[i]; }
            cout << "Instance " << i << " → parties served: " << s
                 << ", total time served: " << secs << "s\n";
        }
        size_t grand_parties; uint64_t grand_secs;
        { lock_guard<mutex> lk(m); grand_parties = total_served_nolock(); grand_secs = grand_total_secs_nolock(); }
        cout << "Total parties served: " << grand_parties << "\n";
        cout << "Total time served: " << grand_secs << " seconds\n";
        cout << "Unmatched Players: " << unmatched_total << "\n";
        cout << "Unmatched Tanks: " << unmatched_tanks << "\n";
        cout << "Unmatched Healers: " << unmatched_healers << "\n";
        cout << "Unmatched DPS: " << unmatched_dps << "\n";
        cout.flush();
    }
};

// worker thread
static void instance_worker(size_t id, Shared& S) {
    unique_lock<mutex> lk(S.m);
    S.idle.push_back(id);
    S.cv.notify_one();

    for (;;) {
        S.cv.wait(lk, [&]{ return S.has_job[id] || S.shutdown; });
        if (S.shutdown && !S.has_job[id]) break;

        const int secs = S.job_secs[id];
        S.has_job[id] = false;
        lk.unlock();

        this_thread::sleep_for(chrono::seconds(secs));

        lk.lock();
        S.active[id] = false;
        S.served[id] += 1;
        S.total_secs[id] += static_cast<uint64_t>(secs);
        S.idle.push_back(id);
        lk.unlock();
        S.cv.notify_all();

        S.print_status_snapshot("Status change: instance finished a party");

        lk.lock();
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // inputs with prompts + validation
    long long n_ll=0, t_ll=0, h_ll=0, d_ll=0, t1_ll=0, t2_ll=0;

    if (!read_one_value<long long>("Please enter n (maximum number of concurrent instances): ",
                                   "n", n_ll, "integer >= 0", [](long long v){ return v >= 0; })) return 1;
    if (!read_one_value<long long>("Please enter t (number of tanks in queue): ",
                                   "t", t_ll, "integer >= 0", [](long long v){ return v >= 0; })) return 1;
    if (!read_one_value<long long>("Please enter h (number of healers in queue): ",
                                   "h", h_ll, "integer >= 0", [](long long v){ return v >= 0; })) return 1;
    if (!read_one_value<long long>("Please enter d (number of DPS in queue): ",
                                   "d", d_ll, "integer >= 0", [](long long v){ return v >= 0; })) return 1;
    if (!read_one_value<long long>("Please enter t1 (minimum clear time in seconds): ",
                                   "t1", t1_ll, "integer >= 0", [](long long v){ return v >= 0; })) return 1;
    if (!read_one_value<long long>("Please enter t2 (maximum clear time in seconds, must be >= t1 and <= 15): ",
                                   "t2", t2_ll, "integer >= 0 and <= 15", [](long long v){ return v >= 0 && v <= 15; })) return 1;

    size_t n = static_cast<size_t>(n_ll);
    size_t tanks = static_cast<size_t>(t_ll);
    size_t healers = static_cast<size_t>(h_ll);
    size_t dps = static_cast<size_t>(d_ll);
    int t1 = static_cast<int>(t1_ll);
    int t2 = static_cast<int>(t2_ll);

    if (t1 > t2) {
        cerr << "t1 (" << t1 << ") is greater than t2 (" << t2 << "). Swapping to satisfy t1 <= t2.\n";
        std::swap(t1, t2);
    }

    // determine parties & unmatched counts
    const size_t parties = min({tanks, healers, dps / 3});
    const size_t unmatched_tanks  = tanks  - parties;
    const size_t unmatched_healers= healers- parties;
    const size_t unmatched_dps    = dps    - 3 * parties;
    const size_t unmatched_total  = unmatched_tanks + unmatched_healers + unmatched_dps;

    // deadlock guard: handle n == 0 
    if (n == 0) {
        if (parties == 0) {
            cout << "\nConfig accepted:\n";
            cout << "  instances=0, tanks=" << tanks
                 << ", healers=" << healers << ", dps=" << dps
                 << ", t1=" << t1 << "s, t2=" << t2 << "s\n";
            cout << "  Total parties to run: 0\n";
            cout << "  Unmatched (cannot form full parties): Tanks=" << unmatched_tanks
                 << ", Healers=" << unmatched_healers << ", DPS=" << unmatched_dps
                 << " (Total=" << unmatched_total << ")\n\n";
            cout << "No instances available and no runnable parties. Exiting.\n";
            return 0;
        } else {
            cerr << "\nError: " << parties
                 << " full parties can be formed, but n = 0 (no instances available).\n"
                 << "Cannot schedule parties without instances. Exiting.\n";
            return 1;
        }
    }

    // shared state init
    Shared S;
    S.n = n;
    S.active.assign(n, false);
    S.has_job.assign(n, false);
    S.job_secs.assign(n, 0);
    S.served.assign(n, 0);
    S.total_secs.assign(n, 0);
    S.total_parties = parties;
    S.scheduled_parties = 0;
    S.shutdown = false;
    S.in_tanks = tanks; S.in_healers = healers; S.in_dps = dps;
    S.unmatched_tanks = unmatched_tanks;
    S.unmatched_healers = unmatched_healers;
    S.unmatched_dps = unmatched_dps;
    S.unmatched_total = unmatched_total;

    {
        lock_guard<mutex> lg(S.out_m);
        cout << "\nConfig accepted:\n";
        cout << "  instances=" << n << ", tanks=" << tanks
             << ", healers=" << healers << ", dps=" << dps
             << ", t1=" << t1 << "s, t2=" << t2 << "s\n";
        cout << "  Total parties to run: " << S.total_parties << "\n";
        cout << "  Unmatched (cannot form full parties): Tanks=" << unmatched_tanks
             << ", Healers=" << unmatched_healers << ", DPS=" << unmatched_dps
             << " (Total=" << unmatched_total << ")\n\n";
    }

    // start workers 
    vector<jthread> threads; threads.reserve(n);
    for (size_t i = 0; i < n; ++i) threads.emplace_back(instance_worker, i, std::ref(S));

    S.print_status_snapshot("Initial status");

    // dispatcher (main thread) 
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(t1, t2);

    while (true) {
        unique_lock<mutex> lk(S.m);
        if (S.scheduled_parties >= S.total_parties) break;
        S.cv.wait(lk, [&]{ return !S.idle.empty(); });
        while (!S.idle.empty() && S.scheduled_parties < S.total_parties) {
            size_t id = S.idle.front(); S.idle.pop_front();
            S.job_secs[id] = dist(rng);
            S.has_job[id] = true;
            S.active[id]  = true;
            ++S.scheduled_parties;
        }
        lk.unlock();
        S.cv.notify_all();
        S.print_status_snapshot("Status change: dispatcher assigned parties");
    }

    // wait for completion, then shutdown 
    {
        unique_lock<mutex> lk(S.m);
        S.cv.wait(lk, [&]{ return S.total_served_nolock() == S.total_parties; });
        S.shutdown = true;
    }
    S.cv.notify_all();

    S.print_status_snapshot("Final status");
    S.print_final_summary();
    return 0;
}