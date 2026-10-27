#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

RUN_MODE="ask"
SKIP_SERVERS=0
for arg in "$@"; do
    case "$arg" in
        --run) RUN_MODE="yes" ;;
        --no-run) RUN_MODE="no" ;;
        --skip-servers) SKIP_SERVERS=1 ;;
        -h|--help) echo "Usage: ./setup.sh [--run] [--no-run] [--skip-servers]"; exit 0 ;;
        *) echo "Unknown option: $arg (try --help)"; exit 2 ;;
    esac
done

FTXUI_TAG="v5.0.0"
FTXUI_REPO="https://github.com/ArthurSonzogni/FTXUI.git"
FTXUI_TARBALL="https://codeload.github.com/ArthurSonzogni/FTXUI/tar.gz/refs/tags/${FTXUI_TAG}"

step() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }
ok()   { printf '\033[1;32m[ok]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[warn]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

have() { command -v "$1" >/dev/null 2>&1; }
have_sbin() { have "$1" || [ -x "/usr/sbin/$1" ] || [ -x "/sbin/$1" ]; }

OS="$(uname -s)"

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    if have sudo; then SUDO="sudo"; fi
fi

run_root() {
    if [ "$(id -u)" -eq 0 ]; then "$@"
    elif [ -n "$SUDO" ]; then $SUDO "$@"
    else die "Need root to install packages, but 'sudo' is not available. Re-run as root."
    fi
}

missing=()
have c++ || have g++ || have clang++ || missing+=("C++ compiler")
have make    || missing+=("make")
have cmake   || missing+=("cmake")
have git     || missing+=("git")
have curl    || missing+=("curl")
have python3 || missing+=("python3")

if [ "$OS" = "Linux" ]; then
    have_sbin iptables || missing+=("iptables")
    have ping          || missing+=("ping")
fi

step "Checking dependencies"
if [ "${#missing[@]}" -eq 0 ]; then
    ok "All required tools are already installed"
elif [ "$OS" = "Darwin" ]; then
    warn "Missing: ${missing[*]}"
    if ! xcode-select -p >/dev/null 2>&1; then
        echo "Installing Xcode Command Line Tools (a dialog will appear)..."
        xcode-select --install || true
        die "Finish the Xcode tools installation, then run ./setup.sh again."
    fi
    if printf '%s\n' "${missing[@]}" | grep -qx "cmake"; then
        have brew || die "cmake is missing and Homebrew was not found. Install Homebrew (https://brew.sh) and re-run."
        brew install cmake
    fi
    ok "Dependencies ready"
elif [ "$OS" = "Linux" ]; then
    warn "Missing: ${missing[*]}"
    . /etc/os-release 2>/dev/null || true
    distro="${ID:-unknown} ${ID_LIKE:-}"
    case " $distro " in
        *debian*|*ubuntu*)
            run_root apt-get update
            run_root apt-get install -y build-essential cmake git curl ca-certificates iptables iputils-ping python3 ;;
        *fedora*|*rhel*|*centos*)
            run_root dnf install -y gcc-c++ make cmake git curl ca-certificates iptables-nft iputils python3 ;;
        *arch*)
            run_root pacman -S --needed --noconfirm base-devel cmake git curl ca-certificates iptables iputils python ;;
        *suse*)
            run_root zypper --non-interactive install gcc-c++ make cmake git curl ca-certificates iptables iputils python3 ;;
        *)
            die "Unsupported distro '${ID:-unknown}'. Please install: a C++17 compiler, make, cmake, git, curl, iptables, ping, python3 - then re-run." ;;
    esac
    ok "Dependencies installed"
else
    die "Unsupported OS: $OS"
fi

step "Preparing FTXUI ${FTXUI_TAG}"
if [ -f deps/FTXUI/CMakeLists.txt ]; then
    ok "deps/FTXUI already present"
else
    mkdir -p deps
    rm -rf deps/FTXUI
    got=0
    export GIT_HTTP_LOW_SPEED_LIMIT=1000 GIT_HTTP_LOW_SPEED_TIME=30
    for attempt in 1 2 3; do
        echo "git clone attempt $attempt/3..."
        if git clone --depth 1 --branch "$FTXUI_TAG" "$FTXUI_REPO" deps/FTXUI; then got=1; break; fi
        rm -rf deps/FTXUI
        sleep 2
    done
    if [ "$got" -eq 0 ]; then
        warn "git clone failed; trying tarball download"
        mkdir -p deps/FTXUI
        curl -fL --connect-timeout 20 --max-time 300 "$FTXUI_TARBALL" \
            | tar -xz --strip-components=1 -C deps/FTXUI \
            || { rm -rf deps/FTXUI; die "Could not download FTXUI. Check your internet connection and re-run."; }
    fi
    [ -f deps/FTXUI/CMakeLists.txt ] || die "FTXUI download looks incomplete"
    ok "FTXUI downloaded"
fi

if [ "$SKIP_SERVERS" -eq 0 ]; then
    step "Fetching S2e server IPs"
    if python3 tools/update_servers.py --write; then
        ok "src/servers.hpp updated with real relay IPs"
    else
        warn "Could not update server IPs - placeholders stay in place (ping/blocking won't work)."
        warn "Retry later with: python3 tools/update_servers.py --write   (or see --input in the script)"
    fi
fi

step "Building"
JOBS="$( (nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2) | head -n1 )"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$JOBS"

BIN="$PWD/build/s2e-server-picker"
[ -x "$BIN" ] || die "Build finished but $BIN was not found"
ok "Built: $BIN"

echo
echo "Run it with:  ./build/s2e-server-picker   (it asks for sudo itself)"

if [ "$RUN_MODE" = "ask" ] && [ -t 0 ] && [ -t 1 ]; then
    read -r -p "Launch now? [y/N] " ans || ans=""
    case "$ans" in [yY]*) RUN_MODE="yes" ;; *) RUN_MODE="no" ;; esac
fi
if [ "$RUN_MODE" = "yes" ]; then
    exec "$BIN"
fi
