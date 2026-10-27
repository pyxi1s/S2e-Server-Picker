# Souce 2 Server Picker

Pick which S2e server locations you connect to, right from your terminal.

It blocks or allows the game's relay IPs through your OS firewall and shows live ping for every location, so you can cut the laggy ones and keep the good one. No game files touched, no injection, just firewall rules.

```
 Southeast Asia(SEA)                                    Block all
    Singapore(SG)                           74ms *   Allow
    Bangkok(BKK)                (pending)   61ms     Block
 East Asia(EA)                                          Block all
    Hong Kong(HK)                           90ms     Allow
    Tokyo(TYO)                             111ms     Allow
```

## What you get

- Locations grouped by region, each with its own Allow / Block toggle
- Live ping, refreshed every ~2 seconds and averaged over the last 5 replies, with `*` on the lowest one
- **Block All**, **Allow All** and **Best Only** (keeps just the fastest location) for quick changes
- A `Block all` / `Allow all` button on every region header
- `(pending)` marker on anything you changed but haven't applied yet
- Activity log with what was applied and any errors
- Windows (`netsh`), Linux (`iptables`) and macOS (`pfctl`) support
- Only touches rules it created itself, and one button wipes them all

## Quick start

You need admin rights (Windows) or root (Linux/macOS). The app asks for them itself: a UAC prompt on Windows, `sudo` on Linux/macOS.

### Windows

```bat
setup.bat
```

Installs whatever is missing through `winget` (CMake, Git, MinGW, Python), pulls FTXUI, fetches the real server IPs, builds the app and drops shortcuts on your Desktop and Start Menu. Add `--no-pause` if you don't want the final pause.

The built program is copied to `C:\s2e_server_picker`, so it keeps working even if you delete the source folder. Both shortcuts point there, run as administrator and open in Windows Terminal when it's installed (otherwise a regular console). To recreate them later:

```bat
powershell -ExecutionPolicy Bypass -File tools\create_shortcuts.ps1 -Exe C:\s2e_server_picker\s2e-server-picker.exe
```

### Linux / macOS

```sh
./setup.sh
```

Flags: `--run` launches the app after the build, `--no-run` never asks, `--skip-servers` skips the IP update.

Dependencies are installed for Debian/Ubuntu, Fedora/RHEL, Arch and openSUSE. On macOS you need the Xcode Command Line Tools, plus Homebrew if CMake is missing. Linux also needs `iptables` and `ping`.

## Using it

| Input | What it does |
| --- | --- |
| Up / Down, Tab | Move between locations and buttons |
| Mouse wheel | Scroll the list |
| Click, Enter or Space on `Allow` / `Block` | Toggle that location |
| Region header `Block all` / `Allow all` | Toggle every location in that region |
| **Block All** / **Allow All** | Set every location to Block / Allow |
| **Best Only** | Allow the lowest-ping location, block the rest |
| **Apply Rules** | Write your choices to the firewall |
| **Reset All Rules** | Remove every rule this app created and set everything back to Allow |
| **Quit** or `q` | Exit |

Everything above only changes your selection. Nothing hits the firewall until you press **Apply Rules**.

**Best Only caveat:** it can only compare locations that are being pinged, and locations this app has blocked aren't pinged. To compare everything, run **Allow All** + **Apply Rules** first.

### Reading the ping column

| Shown | Meaning |
| --- | --- |
| `74ms` | Reply time (green under 60 ms, yellow under 120 ms, red above) |
| `timeout` | No reply from any target |
| `blocked` | This app is blocking that location |
| `no data` | No usable ping targets (placeholder ranges) |
| `no ping` | The `ping` command is missing or failed to run |

Ping uses the system `ping` command: one ICMP echo per location per sweep, against up to 3 single-address relay IPs. A single lost reply won't flash `timeout`, the previous average stays up until a second one in a row fails.

## Getting real server IPs

The repo ships with placeholder ranges (`192.0.2.0/28`, a documentation-only block). They're harmless but block nothing and ping is disabled for them. `setup.bat` / `setup.sh` already handle this, but you can also run it yourself:

```sh
python3 tools/update_servers.py --write
```

It reads Valve's Steam Datagram Relay config for S2e (AppID 1422450) and rewrites the region list in `src/servers.hpp`. Rebuild afterwards. Relay IPs change now and then, so if blocking or ping stops working, rerun the script and rebuild.

If the download fails, save the JSON from the URL the script prints and feed it in:

```sh
python3 tools/update_servers.py --input sdr.json --write
```

Run it without `--write` to print the generated C++ block instead of touching the file. Once real data is found, locations with no relay IPs are dropped.

Want to add or remove locations? Edit the `GROUPS` list at the top of `tools/update_servers.py`. Each entry is a POP code prefix plus a display name, and a POP matches when its code equals or starts with the prefix.

## How blocking works

Each location gets its own rule set named after its POP code, so rules can be changed or removed independently.

| OS | Mechanism |
| --- | --- |
| Windows | Outbound block rules named `S2eServerPicker_<id>` via `netsh advfirewall` |
| Linux | A chain `DLPICK_<ID>` hooked into `OUTPUT` via `iptables` |
| macOS | A sub-anchor `s2e_picker/<id>` via `pfctl` |

Good to know:

- Rules are outbound only, and on Linux/macOS they don't survive a reboot.
- On macOS the app adds an `anchor "s2e_picker/*"` line to a copy of `/etc/pf.conf` the first time it blocks something. If you run a custom pf ruleset, add that line yourself.
- Rules from older versions used different ids (`sea`, `eas`, `na`, `eu`, `sa`, `oce`), so reset those with the old version first.

## Building by hand

You'll need a C++17 compiler, CMake 3.14+ and Git. FTXUI v5.0.0 is downloaded during the build.

```sh
make            # configure and build (Release)
make run        # build, then run
make debug      # Debug build in build-debug/
make clean
```

On Windows:

```bat
build.bat [build|debug|run|rebuild|clean]
```

Set `GENERATOR` to force a CMake generator (e.g. `Ninja` or `MinGW Makefiles`). Plain CMake works too:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary ends up at `build/s2e-server-picker` (or `build\Release\s2e-server-picker.exe` with Visual Studio).

## Uninstall

**Windows:** run `C:\s2e_server_picker\uninstall.bat` (or `uninstall.bat` from the source folder). It asks for admin rights, then:

- stops the program if it's running
- removes every firewall rule this app created (`S2eServerPicker_*`)
- removes the Desktop and Start Menu shortcuts
- deletes `C:\s2e_server_picker`

Your source folder stays untouched.

**Linux / macOS:** no installer. Hit **Reset All Rules** in the app, then delete the folder.

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
