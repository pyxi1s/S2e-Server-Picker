#!/usr/bin/env python3
import argparse
import ipaddress
import json
import pathlib
import re
import sys
import urllib.request

APPID = 1422450
URL = f"https://api.steampowered.com/ISteamApps/GetSDRConfig/v1?appid={APPID}"
HEADER = pathlib.Path(__file__).resolve().parent.parent / "src" / "servers.hpp"
PLACEHOLDER = "192.0.2.0/28"

GROUPS = [
    ("Southeast Asia(SEA)", [
        ("sgp", "Singapore(SG)"),
        ("bkk", "Bangkok(BKK)"),
    ]),
    ("East Asia(EA)", [
        ("hkg", "Hong Kong(HK)"),
        ("tyo", "Tokyo(TYO)"),
        ("seo", "Seoul(SEL)"),
    ]),
    ("North America(NA)", [
        ("iad", "Washington D.C.(IAD)"),
        ("atl", "Atlanta(ATL)"),
        ("mia", "Miami(MIA)"),
        ("ord", "Chicago(ORD)"),
        ("dfw", "Dallas(DFW)"),
        ("den", "Denver(DEN)"),
        ("lax", "Los Angeles(LAX)"),
        ("sjc", "San Jose(SJC)"),
        ("sea", "Seattle(SEA)"),
        ("yyz", "Toronto(YYZ)"),
    ]),
    ("Europe(EU)", [
        ("fra", "Frankfurt(FRA)"),
        ("ams", "Amsterdam(AMS)"),
        ("lhr", "London(LHR)"),
        ("par", "Paris(PAR)"),
        ("mad", "Madrid(MAD)"),
        ("mil", "Milan(MIL)"),
        ("vie", "Vienna(VIE)"),
        ("waw", "Warsaw(WAW)"),
        ("sto", "Stockholm(STO)"),
        ("hel", "Helsinki(HEL)"),
    ]),
    ("South America(SA)", [
        ("gru", "Sao Paulo(GRU)"),
        ("scl", "Santiago(SCL)"),
        ("lim", "Lima(LIM)"),
        ("eze", "Buenos Aires(EZE)"),
        ("bog", "Bogota(BOG)"),
    ]),
    ("Oceania(OCE)", [
        ("syd", "Sydney(SYD)"),
        ("mel", "Melbourne(MEL)"),
        ("akl", "Auckland(AKL)"),
    ]),
]


def load(path):
    if path:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    req = urllib.request.Request(URL, headers={"User-Agent": "deadlock-server-picker/0.2"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.load(resp)


def location_for(code):
    for _, locations in GROUPS:
        for prefix, _ in locations:
            if code == prefix or code.startswith(prefix):
                return prefix
    return None


def build_block(pops):
    ips = {prefix: set() for _, locations in GROUPS for prefix, _ in locations}
    unmapped = []
    for code, pop in pops.items():
        prefix = location_for(code)
        relays = [r.get("ipv4") for r in pop.get("relays", []) if r.get("ipv4")]
        if prefix is None:
            if relays:
                unmapped.append(code)
            continue
        for ip in relays:
            try:
                ips[prefix].add(ipaddress.IPv4Address(ip))
            except ValueError:
                pass

    have_real_data = any(ips.values())
    lines = []
    for group, locations in GROUPS:
        for prefix, name in locations:
            addrs = sorted(ips[prefix])
            if have_real_data and not addrs:
                continue
            entries = [f'"{a}/32"' for a in addrs] or [f'"{PLACEHOLDER}"']
            lines.append(f'        {{"{prefix}", "{group}", "{name}",')
            lines.append("         {")
            for i in range(0, len(entries), 4):
                lines.append("          " + ", ".join(entries[i:i + 4]) + ",")
            lines.append(f"         }}, {'false' if addrs else 'true'}}},")
            print(f"{prefix}: {len(addrs)} relay IPs", file=sys.stderr)
    return "\n".join(lines) + "\n", unmapped


def main():
    ap = argparse.ArgumentParser(description="Generate Deadlock relay IPs for src/servers.hpp")
    ap.add_argument("--input", help="read a saved SDR JSON file instead of downloading")
    ap.add_argument("--write", action="store_true", help=f"rewrite {HEADER}")
    args = ap.parse_args()

    try:
        data = load(args.input)
    except Exception as e:
        print(f"Could not load SDR config: {e}\n"
              f"Download {URL} in a browser, save it, and re-run with --input FILE.",
              file=sys.stderr)
        return 1

    pops = data.get("pops")
    if not isinstance(pops, dict) or not pops:
        print("Unexpected JSON: no 'pops' object found.", file=sys.stderr)
        return 1

    block, unmapped = build_block(pops)
    if unmapped:
        print("Unmapped POPs (add to GROUPS if wanted): " + ", ".join(sorted(unmapped)),
              file=sys.stderr)

    if not args.write:
        print(block)
        return 0

    text = HEADER.read_text(encoding="utf-8")
    pattern = re.compile(
        r"(static const std::vector<Region> regions = \{\n).*?(\n    \};)", re.S)
    if not pattern.search(text):
        print("Region list not found in servers.hpp", file=sys.stderr)
        return 1
    HEADER.write_text(pattern.sub(lambda m: m.group(1) + block.rstrip("\n") + m.group(2),
                                  text, count=1), encoding="utf-8")
    print(f"Updated {HEADER}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
