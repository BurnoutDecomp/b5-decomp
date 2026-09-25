"""FX-LADDER NaN commit: CrashPlayManager::HandlePlayerToVehicleImpact @0x822D5928 and the NaN polarity of its
stomp-timer gate.

Extracts the production HandlePlayerToVehicleImpact and its two constants (KF_MIN_CRASH_MAGNITUDE_REACTION,
KF_MIN_TIME_BETWEEN_TRAFFIC_STOMPS) from BrnCrashPlayManager.cpp and runs FxLadderStompTimer.cpp's checks
on the real structs (the ARTIST trace is in the fixture's banner). The console's
`fcmpu timer, 1.0 ; blt out` at 0x822D5A18/1C is not taken on an unordered compare, so a NaN timer goes on
to the owner / species / normal tests; the pre-fix `timer >= 1.0` stopped it.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxladder_stomp_timer.py [--rev <rev>]

`--rev <b5 rev>` reads BrnCrashPlayManager.cpp from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, constant, definition, parse_args  # noqa: E402

CRASHPLAY = "src/GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayManager.cpp"
CONSTANTS = ["KF_MIN_CRASH_MAGNITUDE_REACTION", "KF_MIN_TIME_BETWEEN_TRAFFIC_STOMPS"]


def main():
    args = parse_args()
    source = Tree(args.rev).read(CRASHPLAY).replace("\r\n", "\n")
    chunks = [constant(source, name) for name in CONSTANTS]
    chunks.append(definition(source, "void CrashPlayManager::HandlePlayerToVehicleImpact("))
    sys.exit(compile_and_run("FxLadderStompTimer.cpp", chunks, prefix="brn_fxladder_stomp_"))


if __name__ == "__main__":
    main()
