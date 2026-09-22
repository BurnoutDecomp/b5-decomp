"""FX-AIDRV G03-D2: replay the production AIDriver::GetIndexOfFurthestVehicle (ARTIST @0x8277D2E0).

The last entry farther from the car than the candidate (the reference distance is never raised),
else -1. See AIDrvFurthestVehicle.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aidrv_furthest_vehicle.py [--rev <rev>]
"""
import re
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, definition, parse_args  # noqa: E402

AIDRIVER_UPDATE = "src/GameSource/World/AI/BrnAIDriver_Update.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(AIDRIVER_UPDATE)
    furthest = re.search(r"^    s32 AIDriver::GetIndexOfFurthestVehicle\(", source, re.M)
    if furthest is None:
        raise ValueError("no GetIndexOfFurthestVehicle definition")
    chunks = ["namespace BrnAI {",
              "namespace {", definition(source, "        Vector2 To2DU(Vector3 lVector)"), "}",
              definition(source, "    NearbyVehicle* NearbyVehicles::GetVehiclePointer(s32 liEntry)"),
              definition(source[furthest.start():], "    s32 AIDriver::GetIndexOfFurthestVehicle("),
              "}"]
    sys.exit(compile_and_run("AIDrvFurthestVehicle.cpp", chunks, prefix="brn_aidrv_furthest_"))


if __name__ == "__main__":
    main()
