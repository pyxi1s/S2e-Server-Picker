#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace dsp {

struct PingResult {
    enum class Status {
        Pending,
        Ok,
        Timeout,
        Blocked,
        NoData,
        Error,
    };
    Status status = Status::Pending;
    double ms = 0.0;
};

bool ParsePingOutput(const std::string& output, double& ms);

PingResult PingHost(const std::string& host, int timeout_ms);

std::vector<std::string> PingTargetsFromCidrs(const std::vector<std::string>& cidrs,
                                              size_t max_hosts);

class PingMonitor {
public:
    using Callback = std::function<void()>;

    PingMonitor(std::vector<std::vector<std::string>> hosts_per_region, Callback on_update);
    ~PingMonitor();

    PingMonitor(const PingMonitor&) = delete;
    PingMonitor& operator=(const PingMonitor&) = delete;

    void Start();
    void Stop();

    PingResult Get(size_t region_index) const;
    void SetBlocked(size_t region_index, bool blocked);

private:
    void Loop();
    PingResult Measure(size_t i);

    const std::vector<std::vector<std::string>> hosts_;
    Callback on_update_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<PingResult> results_;
    std::vector<char> blocked_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

}
