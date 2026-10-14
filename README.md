# Deadlock Server Picker

A terminal UI tool for choosing which Deadlock server locations you connect to. It blocks or allows the game's relay IPs through your operating system's firewall, and shows live latency for each location so you can pick the best one.

```
Southeast Asia(SEA)
    Singapore(SG)                74ms *   Allow
    Bangkok(BKK)                 61ms     Allow
East Asia(EA)
    Hong Kong(HK)                90ms     Allow
    Tokyo(TYO)                  111ms     Block
...
```

## Features

- Locations grouped by region (Southeast Asia, East Asia, North America, Europe, South America, Oceania)
- Per-location Allow / Block toggle, plus a per-region "Block all / Allow all" button on each region header
- **Block All**, **Allow All** and **Best Only** (allow only the lowest-ping location) buttons
- Locations whose choice differs from what is applied are marked `(pending)`
- Live ping for every location, refreshed every ~2 seconds and averaged over the last 5 replies; `*` marks the lowest ping
- Activity log showing what was applied and any errors
- Works on Windows (`netsh`), Linux (`iptables`) and macOS (`pfctl`)
- Only touches rules it created itself, and can remove them all with one button

## Requirements

- Administrator rights (Windows) or root (Linux/macOS). The app asks for them itself: a UAC prompt on Windows, `sudo` on Linux/macOS. On Windows the elevated copy opens in a new Windows Terminal window if `wt.exe` is installed, otherwise in a regular console window.
- A C++17 compiler, CMake 3.14+ and Git to build from source
- Python 3 to fetch the real server IPs
- `iptables` and `ping` on Linux

FTXUI v5.0.0 is downloaded automatically during the build.

## Quick start

### Windows

```bat
setup.bat
```

Installs missing tools with `winget` (CMake, Git, MinGW, Python), downloads FTXUI, fetches server IPs and builds the app, then creates Desktop and Start Menu shortcuts. Pass `--no-pause` to skip the final pause.

### Linux / macOS

```sh
./setup.sh
```

Options: `--run` launches the app after building, `--no-run` never asks, `--skip-servers` skips the IP update.

On Linux the script installs dependencies for Debian/Ubuntu, Fedora/RHEL, Arch and openSUSE. On macOS it needs the Xcode Command Line Tools and, if CMake is missing, Homebrew.

### Install location and shortcuts (Windows)

`setup.bat` copies the built program to `C:\deadlock_server_picker`, so it keeps working even if you delete the source folder. It also creates a **Deadlock Server Picker** shortcut on the Desktop and in the Start Menu. Both point to the installed program, are set to run as administrator and open in Windows Terminal when it is installed. To create them again later:

```bat
powershell -ExecutionPolicy Bypass -File tools\create_shortcuts.ps1 -Exe C:\deadlock_server_picker\deadlock-server-picker.exe
```

### Uninstall (Windows)

Run `C:\deadlock_server_picker\uninstall.bat` (or `uninstall.bat` from the source folder). It asks for administrator rights, then:

- stops the program if it is running
- removes every firewall rule created by this app (`DeadlockServerPicker_*`)
- removes the Desktop and Start Menu shortcuts
- deletes `C:\deadlock_server_picker`

The source folder is not touched. On Linux and macOS there is no installer: use **Reset All Rules** in the app, then delete the folder.

## Manual build

```sh
make            # configure and build (Release)
make run        # build, then run
make debug      # Debug build in build-debug/
make clean
```

Windows:

```bat
build.bat [build|debug|run|rebuild|clean]
```

Set `GENERATOR` to force a CMake generator (e.g. `Ninja` or `MinGW Makefiles`).

Or use CMake directly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary is `build/deadlock-server-picker` (or `build\Release\deadlock-server-picker.exe` with Visual Studio).

## Getting real server IPs

The repository ships with placeholder ranges (`192.0.2.0/28`, a documentation-only block). They are harmless but block nothing, and ping is disabled for them. Generate the real data with:

```sh
python3 tools/update_servers.py --write
```

The script reads Valve's Steam Datagram Relay config for Deadlock (AppID 1422450) and rewrites the region list in `src/servers.hpp`. Rebuild afterwards.

If the download fails, save the JSON from the URL it prints in a browser and run:

```sh
python3 tools/update_servers.py --input sdr.json --write
```

Run without `--write` to print the generated C++ block instead of changing the file. Once real data is found, locations with no relay IPs are left out. Relay IPs can change over time, so rerun the script and rebuild if blocking or ping stops working.

To add or remove locations, edit the `GROUPS` list at the top of `tools/update_servers.py`. Each entry is a POP code prefix and a display name, and a POP matches when its code equals or starts with the prefix.

## Usage

| Input | Action |
| --- | --- |
| Up / Down, Tab | Move between locations and buttons |
| Mouse wheel | Move up / down the list |
| Click, Enter or Space on `Allow` / `Block` | Toggle that location |
| Region header `Block all` / `Allow all`    | Toggle every location in that region                                   |
| **Block All** / **Allow All**              | Set every location to Block / Allow (still needs Apply Rules)          |
| **Best Only**                              | Allow only the lowest-ping location, block the rest                    |
| **Apply Rules** | Write the current choices to the firewall |
| **Reset All Rules** | Remove every rule this app created and set all locations back to Allow |
| **Quit** or `q` | Exit |

Toggling only changes the selection. Nothing reaches the firewall until you press **Apply Rules**.

**Best Only** can only compare locations that are currently pinged. Locations blocked by this app are not measured, so run **Allow All** + **Apply Rules** first if you want every location compared.

## Ping

Latency is measured with the system `ping` command (one ICMP echo per location per sweep, using up to 3 single-address relay IPs per location). The displayed value is the average of the last 5 successful replies, and a single lost reply keeps the previous average instead of flashing `timeout`. A location shows:

| Shown | Meaning |
| --- | --- |
| `74ms` | Reply time (green under 60 ms, yellow under 120 ms, red above) |
| `timeout` | No reply from any target |
| `blocked` | The location is blocked by this app |
| `no data` | No usable ping targets (placeholder ranges) |
| `no ping` | The `ping` command is missing or failed to run |

## How blocking works

Each location gets its own rule set, named after its POP code, so rules can be changed or removed independently.

| OS | Mechanism |
| --- | --- |
| Windows | Outbound block rules named `DeadlockServerPicker_<id>` via `netsh advfirewall` |
| Linux | A chain `DLPICK_<ID>` hooked into `OUTPUT` via `iptables` |
| macOS | A sub-anchor `deadlock_picker/<id>` via `pfctl` |

On macOS the app adds an `anchor "deadlock_picker/*"` line to a copy of `/etc/pf.conf` the first time it blocks something, so pf evaluates its rules. If you use a custom pf ruleset, add that line to it yourself.

Rules are outbound only and are not saved across reboots on Linux and macOS. Rules created by older versions of this tool used different ids (`sea`, `eas`, `na`, `eu`, `sa`, `oce`), so reset them with the old version first.

## Project layout

```
src/
  main.cpp        TUI, state and rendering
  firewall.cpp    Windows / Linux / macOS firewall backends and elevation
  ping.cpp        Ping parsing and the background latency monitor
  process.cpp     Shell command runner
  servers.hpp     Location list (generated by tools/update_servers.py)
tools/
  update_servers.py       Fetches relay IPs and rewrites servers.hpp
  create_shortcuts.ps1    Creates the Desktop and Start Menu shortcuts (Windows)
setup.bat / setup.sh  One-click setup
uninstall.bat         Windows uninstaller
build.bat / Makefile / CMakeLists.txt
```
