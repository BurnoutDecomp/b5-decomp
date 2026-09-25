"""FX-LADDER item 5: AICar::IsExtrapolatedRouteGettingOld @0x8276FD50 and the NaN polarity of its drift test.

Extracts the production AICar::IsExtrapolatedRouteGettingOld and AICar::HasValidRoute plus the
two route-age constants from BrnAICar_Update.cpp and runs FxLadderRouteAge.cpp's numeric checks
(the ARTIST trace is in the fixture's banner). The console's `fcmpu ; ble` at 0x8276FDFC is taken
whenever GT is clear -- an ordered drift at or under the limit AND an unordered (NaN) drift -- so a NaN
drift goes on to the route tests; only an ordered drift above the limit returns 1 there.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxladder_route_age.py [--rev <rev>]

`--rev <b5 rev>` reads BrnAICar_Update.cpp from that revision (cccfeed8 is RED on the NaN checks).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, constant, definition, parse_args  # noqa: E402

AICAR_UPDATE = "src/GameSource/World/AI/BrnAICar_Update.cpp"
CONSTANTS = ["KF_ROUTE_OLD_DISTANCE_AI", "KF_ROUTE_OLD_DISTANCE_PLAYER"]
SIGNATURES = [
    "    bool AICar::HasValidRoute() const",
    "    bool AICar::IsExtrapolatedRouteGettingOld()",
]


def main():
    args = parse_args()
    update = Tree(args.rev).read(AICAR_UPDATE)
    chunks = ["namespace BrnAI {", "namespace vpu = rw::math::vpu;"]
    chunks += [constant(update, name) for name in CONSTANTS]
    chunks += [definition(update, signature) for signature in SIGNATURES]
    chunks.append("}")
    sys.exit(compile_and_run("FxLadderRouteAge.cpp", chunks, prefix="brn_fxladder_route_age_"))


if __name__ == "__main__":
    main()
