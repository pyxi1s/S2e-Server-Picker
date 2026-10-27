#!/usr/bin/env bash
set -uo pipefail

cd "$(dirname "$0")"
SELF="$PWD/$(basename "$0")"

ASSUME_YES=0
PURGE=0
for arg in "$@"; do
    case "$arg" in
        -y|--yes) ASSUME_YES=1 ;;
        --purge) PURGE=1 ;;
        -h|--help)
            echo "Usage: ./uninstall.sh [-y] [--purge]"
            echo "  -y, --yes   don't ask for confirmation"
            echo "  --purge     also delete deps/ (downloaded FTXUI)"
            exit 0 ;;
        *) echo "Unknown option: $arg (try --help)"; exit 2 ;;
    esac
done

step() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }
ok()   { printf '\033[1;32m[ok]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[warn]\033[0m %s\n' "$*" >&2; }

have() { command -v "$1" >/dev/null 2>&1; }

OS="$(uname -s)"

if [ "$(id -u)" -ne 0 ]; then
    have sudo || { echo "[error] Need root to remove firewall rules, but 'sudo' is not available." >&2; exit 1; }
    echo "Requesting administrator rights..."
    exec sudo bash "$SELF" "$@"
fi

echo "============================================================"
echo "  S2e Server Picker - uninstall"
echo "============================================================"
echo
echo "This will remove:"
echo "  - every firewall rule created by this app"
echo "  - the build output (build/, build-debug/)"
[ "$PURGE" -eq 1 ] && echo "  - deps/ (downloaded FTXUI)"
echo "Your source files stay untouched."
echo

if [ "$ASSUME_YES" -eq 0 ]; then
    read -r -p "Continue? [y/N] " ans || ans=""
    case "$ans" in [yY]*) ;; *) echo "Cancelled."; exit 0 ;; esac
fi

step "Stopping the program"
pkill -x s2e-server-picker >/dev/null 2>&1 && ok "Stopped" || ok "Not running"

step "Removing firewall rules"
if [ "$OS" = "Linux" ]; then
    IPT=""
    for c in iptables /usr/sbin/iptables /sbin/iptables; do
        if have "$c" || [ -x "$c" ]; then IPT="$c"; break; fi
    done
    if [ -z "$IPT" ]; then
        warn "iptables not found, nothing to remove"
    else
        count=0
        while read -r chain; do
            [ -n "$chain" ] || continue
            while "$IPT" -w -D OUTPUT -j "$chain" 2>/dev/null; do :; done
            "$IPT" -w -F "$chain" 2>/dev/null
            "$IPT" -w -X "$chain" 2>/dev/null
            count=$((count + 1))
        done < <("$IPT" -w -S 2>/dev/null | awk '$1 == "-N" && $2 ~ /^S2EPICKER_/ { print $2 }')
        ok "Removed $count chain(s)"
    fi
elif [ "$OS" = "Darwin" ]; then
    count=0
    while read -r anchor; do
        [ -n "$anchor" ] || continue
        pfctl -a "$anchor" -F rules >/dev/null 2>&1
        count=$((count + 1))
    done < <(pfctl -a s2e_picker -s Anchors 2>/dev/null | sed 's/^[[:space:]]*//')
    ok "Flushed $count anchor(s)"
    # The app hooked its anchor into the loaded ruleset only (not into the file),
    # so reloading the stock config drops it.
    if [ -f /etc/pf.conf ]; then
        pfctl -f /etc/pf.conf >/dev/null 2>&1 && ok "Reloaded /etc/pf.conf" || warn "Could not reload /etc/pf.conf"
    fi
else
    warn "Unsupported OS: $OS (nothing to remove)"
fi

step "Removing build output"
rm -rf build build-debug
if [ "$PURGE" -eq 1 ]; then rm -rf deps; fi
ok "Done"

echo
echo "Uninstalled."
