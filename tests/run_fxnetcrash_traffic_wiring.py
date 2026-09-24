"""FX-NETCRASH (crash parity 2026-09-24): structural checks on TrafficEntityModule call sites this lane
re-derived against the ARTIST asm, where the change is a call's identity rather than arithmetic a
numeric fixture could see:

  1. UpdateParams_DoTimeSlicedLogic 0x827441A4..0x827441B0: `mr r4, r30 (race car) ; mr r3, r28 (the
     active-race-car interface) ; bl 0x82681DF0` -- RCEntityActiveRaceCarOutputInterface::IsRaceCarPlayer
     (the two index asserts, then `maxRaceCarFlags[idx] >> 1 & 1`), whose bool picks the 0.5 / 1.0
     importance (0x827441C4 / 0x827441CC). The PC stood in `GetPlayerActiveRaceCarIndex() == idx` until
     FX-AIBUZZ bodied the accessor (b5 10caff25); relayed by the conductor, re-derived here.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_traffic_wiring.py [--rev <b5 rev>]
"""
import argparse
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, REPO

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"


def read(rel, rev):
    if rev:
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def code_only(text):
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"', ' ', text)


def checks(module):
    rows = []
    body = code_only(definition(module, "void TrafficEntityModule::UpdateParams_DoTimeSlicedLogic("))
    rows.append(("UpdateParams_DoTimeSlicedLogic: lbIsPlayer = IsRaceCarPlayer(leRaceCar) (bl 0x82681DF0 @0x827441AC)",
                 re.search(r"lbIsPlayer\s*=\s*lpActiveRaceCarInterface\s*->\s*IsRaceCarPlayer\s*\(\s*leRaceCar\s*\)",
                           body) is not None))
    rows.append(("UpdateParams_DoTimeSlicedLogic: the GetPlayerActiveRaceCarIndex() == idx stand-in is gone",
                 "GetPlayerActiveRaceCarIndex" not in body))
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    rows = checks(read(MODULE_CPP, args.rev))
    failures = 0
    for name, ok in rows:
        print(("PASS  " if ok else "FAIL  ") + name)
        failures += 0 if ok else 1
    print(f"run_fxnetcrash_traffic_wiring: {len(rows) - failures}/{len(rows)} pass ({failures} fail)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
