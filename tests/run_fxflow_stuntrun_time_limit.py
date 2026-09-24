"""FX-FLOW (crash parity 2026-09-24): HUDMessageLogic's online stunt-run mode length was a guess.

  GenerateOnlineStuntRunLeadingMessages @0x82394920 computes the elapsed time as
  flt_82CDB7B4 - GetModeTimeRemaining() (`lfs` @0x823949A4, `fsubs` @0x823949AC) and announces a new
  leading team only once more than 10 s have passed (`fcmpu ; ble` vs flt_8202AC38). flt_82CDB7B4 is
  0x42B40000 == 90.0f in the image -- the same word OnlineStuntRunMode::Start @0x82339E70 stores as the
  mode's time limit (`lfs` @0x82339F10). The PC constant was 120.0f ("the inferred online stunt-run mode
  length"), which would announce 30 s early.

Value: the constant's float bits are the image word, and it agrees with BrnOnlineStuntRunMode.cpp's
KF_SCORING_GRACE_SECONDS (the same flt_82CDB7B4).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_stuntrun_time_limit.py [--rev <b5 rev>]
"""
import argparse
import re
import struct
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, report

HUD_CPP = "src/GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.cpp"
MODE_CPP = "src/GameSource/GameState/ModeManager/GameModes/BrnOnlineStuntRunMode.cpp"
IMAGE_WORD = 0x42B40000   # flt_82CDB7B4


def literal(text, name):
    match = re.search(r"\b" + name + r"\s*=\s*([0-9.eE+-]+)f?\s*;", code_only(text))
    return float(match.group(1)) if match else None


def bits(value):
    return struct.unpack(">I", struct.pack(">f", value))[0]


def checks(tree):
    hud = literal(tree.read(HUD_CPP), "KF_ONLINE_STUNT_RUN_MODE_TIME_LIMIT")
    mode = literal(tree.read(MODE_CPP), "KF_SCORING_GRACE_SECONDS")
    print("  KF_ONLINE_STUNT_RUN_MODE_TIME_LIMIT = %r, KF_SCORING_GRACE_SECONDS = %r" % (hud, mode))
    yield ("HUDMessageLogic's online stunt-run length is flt_82CDB7B4 (0x42B40000, 90.0f)",
           hud is not None and bits(hud) == IMAGE_WORD)
    yield ("... the same word OnlineStuntRunMode::Start stores as the mode time limit (@0x82339F10)",
           hud is not None and mode is not None and bits(hud) == bits(mode))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    results = list(checks(Tree(args.rev)))
    return report("run_fxflow_stuntrun_time_limit", results, (0, 0), 0)


if __name__ == "__main__":
    sys.exit(main())
