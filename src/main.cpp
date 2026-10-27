#include <algorithm>
#include <cstdio>
#include <ctime>
#include <deque>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "firewall.hpp"
#include "ping.hpp"
#include "servers.hpp"

namespace {

enum class LogLevel { Info, Success, Warning, Error };

struct LogEntry {
    std::string time;
    LogLevel level;
    std::string text;
};

std::string Timestamp() {
    const std::time_t now = std::time(nullptr);
    const std::tm* tm_ptr = std::localtime(&now);
    std::tm tm{};
    if (tm_ptr) tm = *tm_ptr;
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return buf;
}

struct PingLabel {
    std::string text;
    ftxui::Color color;
};

PingLabel FormatPing(const dsp::PingResult& r, bool best) {
    using S = dsp::PingResult::Status;
    using ftxui::Color;
    switch (r.status) {
        case S::Ok: {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.0fms%s", r.ms, best ? " *" : "");
            const Color c = r.ms < 60 ? Color::Green : (r.ms < 120 ? Color::Yellow : Color::Red);
            return {buf, c};
        }
        case S::Timeout: return {"timeout", Color::Red};
        case S::Blocked: return {"blocked", Color::GrayDark};
        case S::NoData:  return {"no data", Color::GrayDark};
        case S::Error:   return {"no ping", Color::Yellow};
        case S::Pending:
        default:         return {"...", Color::GrayDark};
    }
}

}

int main(int argc, char** argv) {
    using namespace ftxui;

    if (!dsp::FirewallManager::IsElevated()) {
        std::fprintf(stderr,
                     "S2e Server Picker needs administrator/root privileges.\n"
                     "Requesting elevation...\n");
        if (dsp::FirewallManager::RelaunchElevated(argc, argv)) {
            return 0;
        }
        std::fprintf(stderr,
                     "\nElevation was not granted, so the program cannot run.\n%s\n",
                     dsp::FirewallManager::ElevationHint().c_str());
        return 1;
    }

    const auto& regions = dsp::GetRegions();
    dsp::FirewallManager firewall;
    const bool elevated = dsp::FirewallManager::IsElevated();

    std::deque<bool> allowed(regions.size(), true);
    std::deque<bool> applied(regions.size(), true);  // state last written to the firewall
    std::vector<LogEntry> logs;

    auto log = [&](LogLevel level, const std::string& text) {
        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.empty()) continue;
            logs.push_back({Timestamp(), level, line});
        }
        if (logs.size() > 500) logs.erase(logs.begin(), logs.begin() + (logs.size() - 500));
    };

    auto screen = ScreenInteractive::Fullscreen();

    std::vector<std::vector<std::string>> ping_hosts;
    size_t regions_with_ping = 0;
    for (const auto& r : regions) {
        auto hosts = r.placeholder ? std::vector<std::string>{}
                                   : dsp::PingTargetsFromCidrs(r.cidrs, 3);
        if (!hosts.empty()) ++regions_with_ping;
        ping_hosts.push_back(std::move(hosts));
    }
    dsp::PingMonitor monitor(std::move(ping_hosts), [&screen] { screen.PostEvent(Event::Custom); });

    log(LogLevel::Info, "Platform: " + dsp::FirewallManager::PlatformName());
    if (elevated) {
        log(LogLevel::Success, "Elevated privileges detected.");
    } else {
        log(LogLevel::Error, "Not running as Administrator/root. " + dsp::FirewallManager::ElevationHint());
    }
    size_t placeholder_count = 0;
    for (const auto& r : regions) {
        if (r.placeholder) ++placeholder_count;
    }
    if (placeholder_count > 0) {
        log(LogLevel::Warning,
            std::to_string(placeholder_count) + " location(s) still use PLACEHOLDER ranges (see servers.hpp).");
    }
    if (regions_with_ping == 0) {
        log(LogLevel::Warning,
            "Ping disabled: no real server IPs yet. Run: python3 tools/update_servers.py --write");
    }
    {
        const auto blocked = firewall.QueryBlocked(regions);
        size_t blocked_count = 0;
        for (size_t i = 0; i < regions.size(); ++i) {
            if (!blocked[i]) continue;
            allowed[i] = false;
            applied[i] = false;
            monitor.SetBlocked(i, true);
            ++blocked_count;
        }
        if (blocked_count > 0) {
            log(LogLevel::Info, std::to_string(blocked_count) + " location(s) are already blocked by existing rules.");
        }
    }
    log(LogLevel::Info, "Click Allow/Block to toggle. Press 'Apply Rules' to commit.");

    auto group_all_allowed = [&allowed](const std::vector<size_t>& members) {
        return std::all_of(members.begin(), members.end(), [&](size_t j) { return allowed[j]; });
    };

    Components toggles;
    Components ordered;  // focus order: group button, then its locations
    Components group_button_at(regions.size());
    for (size_t i = 0; i < regions.size(); ++i) {
        if (i == 0 || regions[i].group != regions[i - 1].group) {
            std::vector<size_t> members;
            for (size_t j = i; j < regions.size() && regions[j].group == regions[i].group; ++j) {
                members.push_back(j);
            }
            ButtonOption gopt;
            gopt.transform = [group_all_allowed, members](const EntryState& state) {
                Element e = text(group_all_allowed(members) ? " Block all " : " Allow all ") | bold |
                            color(Color::Cyan);
                if (state.focused) e = e | inverted;
                return e;
            };
            auto gbtn = Button("", [&allowed, group_all_allowed, members] {
                const bool block = group_all_allowed(members);
                for (size_t j : members) allowed[j] = !block;
            }, gopt);
            group_button_at[i] = gbtn;
            ordered.push_back(gbtn);
        }
        ButtonOption opt;
        opt.transform = [&allowed, i](const EntryState& state) {
            const bool a = allowed[i];
            Element e = text(a ? " Allow " : " Block ") | bold | color(a ? Color::Green : Color::Red);
            if (state.focused) e = e | inverted;
            return e;
        };
        toggles.push_back(Button("", [&allowed, i] { allowed[i] = !allowed[i]; }, opt));
        ordered.push_back(toggles.back());
    }
    auto region_list = Container::Vertical(ordered);

    auto on_apply = [&] {
        if (!elevated) {
            log(LogLevel::Error, "Cannot apply rules without elevated privileges. " +
                                     dsp::FirewallManager::ElevationHint());
            return;
        }
        log(LogLevel::Info, "Applying rules...");
        int ok = 0, failed = 0;
        for (size_t i = 0; i < regions.size(); ++i) {
            const auto& r = regions[i];
            const auto res = allowed[i] ? firewall.UnblockRegion(r) : firewall.BlockRegion(r);
            log(res.ok ? LogLevel::Success : LogLevel::Error, r.name + ": " + res.message);
            if (res.ok) {
                monitor.SetBlocked(i, !allowed[i]);
                applied[i] = allowed[i];
            }
            (res.ok ? ok : failed)++;
        }
        log(failed == 0 ? LogLevel::Success : LogLevel::Warning,
            "Done: " + std::to_string(ok) + " succeeded, " + std::to_string(failed) + " failed.");
    };

    auto on_reset = [&] {
        if (!elevated) {
            log(LogLevel::Error, "Cannot reset rules without elevated privileges. " +
                                     dsp::FirewallManager::ElevationHint());
            return;
        }
        log(LogLevel::Info, "Removing all rules created by this app...");
        const auto res = firewall.ResetAll(regions);
        log(res.ok ? LogLevel::Success : LogLevel::Error, res.message);
        if (res.ok) {
            for (auto& a : allowed) a = true;
            for (auto& a : applied) a = true;
            for (size_t i = 0; i < regions.size(); ++i) monitor.SetBlocked(i, false);
        }
    };

    auto set_all = [&](bool value) {
        for (auto& a : allowed) a = value;
        log(LogLevel::Info, std::string(value ? "All locations set to Allow" : "All locations set to Block") +
                                ". Press 'Apply Rules' to commit.");
    };

    auto best_only = [&] {
        int best_i = -1;
        double best_ms = 0.0;
        size_t blocked_unmeasured = 0;
        for (size_t i = 0; i < regions.size(); ++i) {
            const auto p = monitor.Get(i);
            if (p.status == dsp::PingResult::Status::Blocked) ++blocked_unmeasured;
            if (p.status != dsp::PingResult::Status::Ok) continue;
            if (best_i < 0 || p.ms < best_ms) {
                best_i = static_cast<int>(i);
                best_ms = p.ms;
            }
        }
        if (best_i < 0) {
            log(LogLevel::Warning, "Best Only: no ping results yet, nothing changed.");
            return;
        }
        for (size_t i = 0; i < regions.size(); ++i) allowed[i] = (static_cast<int>(i) == best_i);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f", best_ms);
        log(LogLevel::Info, "Best Only: " + regions[static_cast<size_t>(best_i)].name + " (" + buf +
                                "ms) allowed, all others set to Block. Press 'Apply Rules' to commit.");
        if (blocked_unmeasured > 0) {
            log(LogLevel::Warning,
                "Locations blocked by this app are not pinged, so they were not compared. "
                "Use Allow All + Apply Rules first to compare every location.");
        }
    };

    auto buttons = Container::Horizontal({
        Button("Block All", [&] { set_all(false); }),
        Button("Allow All", [&] { set_all(true); }),
        Button("Best Only", best_only),
        Button("Apply Rules", on_apply),
        Button("Reset All Rules", on_reset),
        Button("Quit", screen.ExitLoopClosure()),
    });

    auto root = Container::Vertical({region_list, buttons});

    root |= CatchEvent([&](Event event) {
        if (event.is_mouse()) {
            if (event.mouse().button == Mouse::WheelDown) return region_list->OnEvent(Event::ArrowDown);
            if (event.mouse().button == Mouse::WheelUp) return region_list->OnEvent(Event::ArrowUp);
            return false;
        }
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        // Space acts like Enter on the focused button
        if (event == Event::Character(' ')) {
            return root->OnEvent(Event::Return);
        }
        return false;
    });

    auto renderer = Renderer(root, [&] {
        Element banner =
            elevated
                ? text(" [OK] Elevated privileges - firewall changes are possible ") | color(Color::Green)
                : text(" [!] NOT elevated - " + dsp::FirewallManager::ElevationHint() + " ") |
                      color(Color::White) | bgcolor(Color::Red) | bold;

        std::vector<dsp::PingResult> pings;
        int best = -1;
        for (size_t i = 0; i < regions.size(); ++i) {
            pings.push_back(monitor.Get(i));
            if (pings[i].status == dsp::PingResult::Status::Ok &&
                (best < 0 || pings[i].ms < pings[static_cast<size_t>(best)].ms)) {
                best = static_cast<int>(i);
            }
        }

        Elements rows;
        std::string current_group;
        for (size_t i = 0; i < regions.size(); ++i) {
            const auto& r = regions[i];
            if (r.group != current_group) {
                current_group = r.group;
                Element header = text(" " + r.group) | bold | flex;
                rows.push_back(group_button_at[i] ? hbox({header, group_button_at[i]->Render()})
                                                  : header);
            }
            const PingLabel ping = FormatPing(pings[i], static_cast<int>(i) == best);
            const bool pending = allowed[i] != applied[i];
            Element row = hbox({
                text("    " + r.name) | flex,
                text(pending ? "(pending) " : "") | color(Color::Yellow) | size(WIDTH, EQUAL, 10),
                text(" " + ping.text + " ") | color(ping.color) | size(WIDTH, EQUAL, 11),
                toggles[i]->Render(),
            });
            rows.push_back(row);
        }

        size_t pending_count = 0;
        for (size_t i = 0; i < regions.size(); ++i) {
            if (allowed[i] != applied[i]) ++pending_count;
        }
        Element pending_line =
            pending_count > 0
                ? text(" " + std::to_string(pending_count) +
                       " unapplied change(s) - press 'Apply Rules' ") |
                      color(Color::Yellow) | bold | hcenter
                : text("");

        Elements log_lines;
        for (size_t i = 0; i < logs.size(); ++i) {
            const auto& e = logs[i];
            Color c = Color::White;
            std::string tag;
            switch (e.level) {
                case LogLevel::Info:    c = Color::Cyan;   tag = "INFO "; break;
                case LogLevel::Success: c = Color::Green;  tag = " OK   "; break;
                case LogLevel::Warning: c = Color::Yellow; tag = "WARN "; break;
                case LogLevel::Error:   c = Color::Red;    tag = "ERROR"; break;
            }
            Element line = hbox({
                text("[" + e.time + "] ") | dim,
                text(tag + " ") | bold | color(c),
                paragraph(e.text) | flex,
            });
            if (i + 1 == logs.size()) line = line | focus;
            log_lines.push_back(line);
        }

        return vbox({
            text(" S2E SERVER PICKER ") | bold | hcenter,
            separator(),
            banner,
            hbox({
                window(text(" Regions "), vbox(std::move(rows)) | vscroll_indicator | yframe | yflex) |
                    size(WIDTH, EQUAL, 74) | yflex,
                window(text(" Activity log "),
                       vbox(std::move(log_lines)) | vscroll_indicator | yframe | flex) |
                    flex,
            }) | flex,
            pending_line,
            hbox({buttons->Render() | hcenter, filler()}) | center,
            text(" Arrows/Tab: navigate   Space/Enter: toggle/press   q: quit ") | dim | hcenter,
        });
    });

    monitor.Start();
    screen.Loop(renderer);
    monitor.Stop();
    return 0;
}
