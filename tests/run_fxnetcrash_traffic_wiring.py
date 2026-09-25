"""FX-NETCRASH (crash parity 2026-09-24): structural checks on TrafficEntityModule call sites this lane
re-derived against the ARTIST asm, where the change is a call's identity rather than arithmetic a
numeric fixture could see:

  1. UpdateParams_DoTimeSlicedLogic 0x827441A4..0x827441B0: `mr r4, r30 (race car) ; mr r3, r28 (the
     active-race-car interface) ; bl 0x82681DF0` -- RCEntityActiveRaceCarOutputInterface::IsRaceCarPlayer
     (the two index asserts, then `maxRaceCarFlags[idx] >> 1 & 1`), whose bool picks the 0.5 / 1.0
     importance (0x827441C4 / 0x827441CC). The PC stood in `GetPlayerActiveRaceCarIndex() == idx` until
     FX-AIBUZZ bodied the accessor (b5 10caff25); relayed by the conductor, re-derived here.
  2. UpdateRecoveringFromSlam @0x8273E778 (item 4, FX-TRAFFIC5 follow-up): 0x8273E87C `bl
     GetTrafficPhysicsInfoForVehicl` is followed at once by `lfs f0, 0xFE0(r3)` -- the console has no
     `lpPhysInfo` tripwire and no null test there, and no behaviour switch anywhere in the 85 words. The
     PC carried both, plus the BRN_TRAFFIC_NO_SLAM_DRIVE env A/B that could skip the pedal stores.

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

    slam_text = definition(module, "void TrafficEntityModule::UpdateRecoveringFromSlam(")
    slam = code_only(slam_text)
    rows.append(("UpdateRecoveringFromSlam: no BRN_TRAFFIC_NO_SLAM_DRIVE behaviour switch (none in 0x8273E778..0x8273E8C8)",
                 "BRN_TRAFFIC_NO_SLAM_DRIVE" not in slam_text))
    rows.append(("UpdateRecoveringFromSlam: no null test on the physics info (0x8273E87C bl -> 0x8273E884 lfs 0xFE0(r3))",
                 re.search(r"if\s*\(\s*lpInfo\s*==\s*0\s*\)", slam) is None))
    rows.append(("UpdateRecoveringFromSlam: no invented `lpPhysInfo` tripwire (the console asserts only :16732/:16735/:16736)",
                 '"lpPhysInfo"' not in slam_text))
    rows.append(("UpdateRecoveringFromSlam: pedals still stored from +0xFE0 / +0xFDC (fsel 0x8273E88C / 0x8273E898)",
                 re.search(r"lpControls\s*->\s*mfGas\s*=", slam) is not None
                 and re.search(r"lpControls\s*->\s*mfBrake\s*=", slam) is not None
                 and re.search(r"lpControls\s*->\s*mfSteering\s*=\s*lpInfo\s*->\s*mfSteeringDirection", slam) is not None))
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
