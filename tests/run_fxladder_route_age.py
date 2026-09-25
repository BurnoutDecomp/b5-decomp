"""FX-LADDER item 5: AICar::IsExtrapolatedRouteGettingOld @0x8276FD50 reads a NaN drift as "old".

Extracts the production AICar::IsExtrapolatedRouteGettingOld and AICar::HasValidRoute plus the
two route-age constants from BrnAICar_Update.cpp and runs FxLadderRouteAge.cpp's numeric checks
(the ARTIST trace is in the fixture's banner). The console's `fcmpu ; ble` at 0x8276FDFC only
continues for an ORDERED drift at or under the limit, so a NaN drift returns 1.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxladder_route_age.py [--rev <rev>]

`--rev <b5 rev>` reads BrnAICar_Update.cpp from that revision (the RED side of the fix).
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
