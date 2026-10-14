#include "ping.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <future>
#include <numeric>

#include "process.hpp"

namespace dsp {
namespace {

// Number of recent replies averaged into the displayed latency.
constexpr size_t kPingWindow = 5;

bool IsIpv4Literal(const std::string& s) {
    if (s.empty() || s.size() > 15) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isdigit(c) || c == '.';
    });
}

}

bool ParsePingOutput(const std::string& output, double& ms) {
    std::string lower = output;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower.find("ttl=") == std::string::npos) return false;

    size_t pos = 0;
    while ((pos = lower.find("ms", pos)) != std::string::npos) {
        size_t end = pos;
        while (end > 0 && lower[end - 1] == ' ') --end;
        size_t start = end;
        while (start > 0 && (std::isdigit(static_cast<unsigned char>(lower[start - 1])) ||
                             lower[start - 1] == '.')) {
            --start;
        }
        if (end > start) {
            ms = std::strtod(lower.substr(start, end - start).c_str(), nullptr);
            return true;
        }
        pos += 2;
    }
    return false;
}

PingResult PingHost(const std::string& host, int timeout_ms) {
    PingResult result;
    if (!IsIpv4Literal(host)) {
        result.status = PingResult::Status::Error;
        return result;
    }

#if defined(_WIN32)
    const std::string cmd =
        "ping -n 1 -w " + std::to_string(timeout_ms) + " " + host;
#elif defined(__APPLE__)
    const std::string cmd =
        "ping -n -c 1 -W " + std::to_string(timeout_ms) + " " + host;
#else
    const int seconds = std::max(1, (timeout_ms + 999) / 1000);
    const std::string cmd =
        "ping -n -c 1 -W " + std::to_string(seconds) + " " + host;
#endif

    const auto res = RunCommand(cmd);
    double ms = 0.0;
    if (ParsePingOutput(res.output, ms)) {
        result.status = PingResult::Status::Ok;
        result.ms = ms;
    } else if (res.exit_code == 127 || res.exit_code == -1 ||
               res.output.find("not found") != std::string::npos ||
               res.output.find("not recognized") != std::string::npos) {
        result.status = PingResult::Status::Error;
    } else {
        result.status = PingResult::Status::Timeout;
    }
    return result;
}

std::vector<std::string> PingTargetsFromCidrs(const std::vector<std::string>& cidrs,
                                              size_t max_hosts) {
    std::vector<std::string> hosts;
    for (const auto& c : cidrs) {
        if (hosts.size() >= max_hosts) break;
        std::string addr = c;
        const auto slash = c.find('/');
        if (slash != std::string::npos) {
            if (c.substr(slash + 1) != "32") continue;
            addr = c.substr(0, slash);
        }
        if (IsIpv4Literal(addr)) hosts.push_back(addr);
    }
    return hosts;
}

PingMonitor::PingMonitor(std::vector<std::vector<std::string>> hosts_per_region,
                         Callback on_update)
    : hosts_(std::move(hosts_per_region)),
      on_update_(std::move(on_update)),
      results_(hosts_.size()),
      blocked_(hosts_.size(), 0),
      samples_(hosts_.size()),
      fails_(hosts_.size(), 0) {
    for (size_t i = 0; i < hosts_.size(); ++i) {
        if (hosts_[i].empty()) results_[i].status = PingResult::Status::NoData;
    }
}

PingMonitor::~PingMonitor() { Stop(); }

void PingMonitor::Start() {
    if (thread_.joinable()) return;
    stop_ = false;
    thread_ = std::thread(&PingMonitor::Loop, this);
}

void PingMonitor::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

PingResult PingMonitor::Get(size_t i) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return i < results_.size() ? results_[i] : PingResult{};
}

void PingMonitor::SetBlocked(size_t i, bool blocked) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (i >= blocked_.size()) return;
    blocked_[i] = blocked ? 1 : 0;
    samples_[i].clear();
    fails_[i] = 0;
    if (blocked) {
        results_[i].status = PingResult::Status::Blocked;
    } else if (results_[i].status == PingResult::Status::Blocked) {
        results_[i].status = PingResult::Status::Pending;
    }
}

PingResult PingMonitor::Measure(size_t i) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (blocked_[i]) return PingResult{PingResult::Status::Blocked, 0.0};
    }
    if (hosts_[i].empty()) return PingResult{PingResult::Status::NoData, 0.0};

    PingResult last{PingResult::Status::Timeout, 0.0};
    for (const auto& host : hosts_[i]) {
        if (stop_) break;
        last = PingHost(host, 1000);
        if (last.status == PingResult::Status::Ok ||
            last.status == PingResult::Status::Error) {
            break;
        }
    }
    return last;
}

void PingMonitor::Loop() {
    using namespace std::chrono_literals;
    while (!stop_) {
        std::vector<std::future<PingResult>> jobs;
        jobs.reserve(hosts_.size());
        for (size_t i = 0; i < hosts_.size(); ++i) {
            jobs.push_back(std::async(std::launch::async, [this, i] { return Measure(i); }));
        }
        for (size_t i = 0; i < jobs.size(); ++i) {
            PingResult r = jobs[i].get();
            std::lock_guard<std::mutex> lock(mutex_);
            if (blocked_[i] && r.status != PingResult::Status::Blocked) continue;
            auto& samples = samples_[i];
            if (r.status == PingResult::Status::Ok) {
                fails_[i] = 0;
                samples.push_back(r.ms);
                while (samples.size() > kPingWindow) samples.pop_front();
                r.ms = std::accumulate(samples.begin(), samples.end(), 0.0) /
                       static_cast<double>(samples.size());
            } else if (r.status == PingResult::Status::Timeout && !samples.empty() &&
                       ++fails_[i] < 2) {
                continue;  // tolerate a single lost reply, keep showing the average
            } else {
                samples.clear();
                fails_[i] = 0;
            }
            results_[i] = r;
        }
        if (on_update_) on_update_();

        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, 2s, [this] { return stop_.load(); });
    }
}

}
