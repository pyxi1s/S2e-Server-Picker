#include "firewall.hpp"
#include "process.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#  include <windows.h>
#  include <shellapi.h>
#  define DSP_SUPPORTED 1
#elif defined(__linux__) || defined(__APPLE__)
#  include <sys/types.h>
#  include <unistd.h>
#  if defined(__APPLE__)
#    include <mach-o/dyld.h>
#  endif
#  define DSP_SUPPORTED 1
#else
#  define DSP_SUPPORTED 0
#endif

namespace dsp {
namespace {

#if DSP_SUPPORTED

bool IsSafeTarget(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isxdigit(c) || c == '.' || c == ':' || c == '/';
    });
}

bool IsSafeId(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::islower(c) || std::isdigit(c);
    });
}

[[maybe_unused]] std::string Join(const std::vector<std::string>& v, size_t begin, size_t end,
                                  const char* sep) {
    std::string out;
    for (size_t i = begin; i < end; ++i) {
        if (i != begin) out += sep;
        out += v[i];
    }
    return out;
}

OperationResult CheckPreconditions(const Region& r, bool need_ranges) {
    if (!FirewallManager::IsElevated())
        return {false, "Missing privileges. " + FirewallManager::ElevationHint()};
    if (!IsSafeId(r.id))
        return {false, "Invalid region id '" + r.id + "' (use a-z and 0-9 only)"};
    if (need_ranges) {
        if (r.cidrs.empty()) return {false, "No address ranges configured"};
        for (const auto& c : r.cidrs)
            if (!IsSafeTarget(c)) return {false, "Rejected unsafe address entry '" + c + "'"};
    }
    return {true, ""};
}

#endif

#if defined(_WIN32)

constexpr size_t kChunkSize = 50;

std::string RuleName(const Region& r) { return "DeadlockServerPicker_" + r.id; }

void DeleteRules(const Region& r) {
    RunCommand("netsh advfirewall firewall delete rule name=\"" + RuleName(r) + "\"");
}

OperationResult BlockImpl(const Region& r) {
    DeleteRules(r);
    int rules = 0;
    for (size_t i = 0; i < r.cidrs.size(); i += kChunkSize) {
        const size_t end = std::min(i + kChunkSize, r.cidrs.size());
        const std::string cmd =
            "netsh advfirewall firewall add rule name=\"" + RuleName(r) +
            "\" dir=out action=block enable=yes profile=any protocol=any remoteip=" +
            Join(r.cidrs, i, end, ",");
        const auto res = RunCommand(cmd);
        if (res.exit_code != 0) {
            DeleteRules(r);
            return {false, "netsh failed: " + res.output};
        }
        ++rules;
    }
    return {true, "BLOCKED (" + std::to_string(r.cidrs.size()) + " ranges, " +
                      std::to_string(rules) + " rule(s))"};
}

OperationResult UnblockImpl(const Region& r) {
    DeleteRules(r);
    return {true, "allowed (rules removed)"};
}

#elif defined(__linux__)

std::string ChainName(const Region& r) {
    std::string n = "DLPICK_";
    for (unsigned char c : r.id) n.push_back(static_cast<char>(std::toupper(c)));
    return n;
}

void RemoveChain(const std::string& chain) {
    for (int i = 0; i < 16; ++i) {
        if (RunCommand("iptables -w -D OUTPUT -j " + chain).exit_code != 0) break;
    }
    RunCommand("iptables -w -F " + chain);
    RunCommand("iptables -w -X " + chain);
}

OperationResult BlockImpl(const Region& r) {
    const auto probe = RunCommand("iptables --version");
    if (probe.exit_code != 0) return {false, "iptables not available: " + probe.output};

    const std::string chain = ChainName(r);
    RemoveChain(chain);

    auto fail = [&](const std::string& what, const CommandResult& res) {
        RemoveChain(chain);
        return OperationResult{false, what + ": " + res.output};
    };

    auto res = RunCommand("iptables -w -N " + chain);
    if (res.exit_code != 0) return fail("could not create chain", res);

    for (const auto& cidr : r.cidrs) {
        res = RunCommand("iptables -w -A " + chain + " -d " + cidr + " -j DROP");
        if (res.exit_code != 0) return fail("could not add rule for " + cidr, res);
    }

    res = RunCommand("iptables -w -I OUTPUT 1 -j " + chain);
    if (res.exit_code != 0) return fail("could not hook chain into OUTPUT", res);

    return {true, "BLOCKED (" + std::to_string(r.cidrs.size()) + " ranges, chain " + chain + ")"};
}

OperationResult UnblockImpl(const Region& r) {
    const auto probe = RunCommand("iptables --version");
    if (probe.exit_code != 0) return {false, "iptables not available: " + probe.output};
    RemoveChain(ChainName(r));
    return {true, "allowed (chain removed)"};
}

#elif defined(__APPLE__)

constexpr const char* kAnchorRoot = "deadlock_picker";

std::string AnchorName(const Region& r) { return std::string(kAnchorRoot) + "/" + r.id; }

std::string WriteTempFile(const std::string& content) {
    char path[] = "/tmp/dlpicker.XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0) return "";
    const char* data = content.data();
    size_t left = content.size();
    while (left > 0) {
        const ssize_t n = write(fd, data, left);
        if (n <= 0) { close(fd); std::remove(path); return ""; }
        data += n;
        left -= static_cast<size_t>(n);
    }
    close(fd);
    return path;
}

OperationResult EnsureAnchorHooked() {
    const auto current = RunCommand("pfctl -sr");
    if (current.output.find(kAnchorRoot) != std::string::npos) return {true, ""};

    std::ifstream in("/etc/pf.conf");
    if (!in) return {false, "cannot read /etc/pf.conf"};
    std::stringstream ss;
    ss << in.rdbuf();
    ss << "\n# Added by Deadlock Server Picker\nanchor \"" << kAnchorRoot << "/*\"\n";

    const std::string tmp = WriteTempFile(ss.str());
    if (tmp.empty()) return {false, "cannot create temporary pf.conf"};
    const auto res = RunCommand("pfctl -f \"" + tmp + "\"");
    std::remove(tmp.c_str());
    if (res.exit_code != 0) return {false, "could not load pf ruleset: " + res.output};
    return {true, ""};
}

OperationResult BlockImpl(const Region& r) {
    const auto hook = EnsureAnchorHooked();
    if (!hook.ok) return hook;

    const std::string rules =
        "block drop out quick to { " + Join(r.cidrs, 0, r.cidrs.size(), ", ") + " }\n";
    const std::string tmp = WriteTempFile(rules);
    if (tmp.empty()) return {false, "cannot create temporary rules file"};

    const auto res = RunCommand("pfctl -a \"" + AnchorName(r) + "\" -f \"" + tmp + "\"");
    std::remove(tmp.c_str());
    if (res.exit_code != 0) return {false, "pfctl failed: " + res.output};

    RunCommand("pfctl -e");
    return {true, "BLOCKED (" + std::to_string(r.cidrs.size()) + " ranges, anchor " +
                      AnchorName(r) + ")"};
}

OperationResult UnblockImpl(const Region& r) {
    const auto res = RunCommand("pfctl -a \"" + AnchorName(r) + "\" -F rules");
    if (res.exit_code != 0) return {true, "allowed (nothing to remove)"};
    return {true, "allowed (anchor flushed)"};
}

#endif

}

bool FirewallManager::IsElevated() {
#if defined(_WIN32)
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
#elif defined(__linux__) || defined(__APPLE__)
    return geteuid() == 0;
#else
    return false;
#endif
}

std::string FirewallManager::PlatformName() {
#if defined(_WIN32)
    return "Windows (netsh)";
#elif defined(__linux__)
    return "Linux (iptables)";
#elif defined(__APPLE__)
    return "macOS (pfctl)";
#else
    return "Unsupported";
#endif
}

std::string FirewallManager::ElevationHint() {
#if defined(_WIN32)
    return "Restart your terminal with 'Run as administrator'.";
#else
    return "Restart with sudo.";
#endif
}

bool FirewallManager::RelaunchElevated(int argc, char** argv) {
#if defined(_WIN32)
    (void)argc;
    (void)argv;
    std::wstring path(32768, L'\0');
    const DWORD len = GetModuleFileNameW(nullptr, &path[0], static_cast<DWORD>(path.size()));
    if (len == 0 || len >= path.size()) return false;
    path.resize(len);

    wchar_t wt_path[MAX_PATH];
    const DWORD wt_len = SearchPathW(nullptr, L"wt.exe", nullptr, MAX_PATH, wt_path, nullptr);
    if (wt_len > 0 && wt_len < MAX_PATH) {
        const std::wstring params =
            L"new-tab --title \"Deadlock Server Picker\" \"" + path + L"\"";
        SHELLEXECUTEINFOW wt{};
        wt.cbSize = sizeof(wt);
        wt.lpVerb = L"runas";
        wt.lpFile = wt_path;
        wt.lpParameters = params.c_str();
        wt.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&wt) == TRUE) return true;
        if (GetLastError() == ERROR_CANCELLED) return false;
    }

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.lpVerb = L"runas";
    info.lpFile = path.c_str();
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info) == TRUE;
#elif defined(__linux__) || defined(__APPLE__)
    std::string exe;
#  if defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) exe = buf;
#  else
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) exe.assign(buf, static_cast<size_t>(n));
#  endif
    if (exe.empty() && argc > 0) exe = argv[0];
    if (exe.empty()) return false;

    std::vector<std::string> args = {"sudo", "--", exe};
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    std::vector<char*> cargs;
    for (auto& a : args) cargs.push_back(&a[0]);
    cargs.push_back(nullptr);

    execvp("sudo", cargs.data());
    return false;
#else
    (void)argc;
    (void)argv;
    return false;
#endif
}

OperationResult FirewallManager::BlockRegion(const Region& region) {
#if DSP_SUPPORTED
    if (auto pre = CheckPreconditions(region, true); !pre.ok) return pre;
    return BlockImpl(region);
#else
    (void)region;
    return {false, "This operating system is not supported"};
#endif
}

OperationResult FirewallManager::UnblockRegion(const Region& region) {
#if DSP_SUPPORTED
    if (auto pre = CheckPreconditions(region, false); !pre.ok) return pre;
    return UnblockImpl(region);
#else
    (void)region;
    return {false, "This operating system is not supported"};
#endif
}

OperationResult FirewallManager::ResetAll(const std::vector<Region>& regions) {
#if DSP_SUPPORTED
    OperationResult total{true, ""};
    int removed = 0;
    for (const auto& r : regions) {
        if (auto pre = CheckPreconditions(r, false); !pre.ok) return pre;
        const auto res = UnblockImpl(r);
        if (res.ok) {
            ++removed;
        } else {
            total.ok = false;
            total.message += r.name + ": " + res.message + "\n";
        }
    }
    if (total.ok) total.message = "Removed rules for " + std::to_string(removed) + " region(s)";
    else if (!total.message.empty() && total.message.back() == '\n') total.message.pop_back();
    return total;
#else
    (void)regions;
    return {false, "This operating system is not supported"};
#endif
}

}
