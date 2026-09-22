"""FX-AIDRV G01-D1: replay the production AICar::GetUsefulDirection (ARTIST @0x82770028).

Both gates measure a Y-zeroed copy (stfs 0.0 into the Y lane at 0x827700A4 / 0x827701A0); the
returns are the full 3D velocity (normalised) / facing. See AIDrvUsefulDirection.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aidrv_useful_direction.py [--rev <rev>]
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, constant, definition, parse_args  # noqa: E402

AICAR_UPDATE = "src/GameSource/World/AI/BrnAICar_Update.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(AICAR_UPDATE)
    chunks = ["namespace BrnAI {", "namespace vpu = rw::math::vpu;",
              constant(source, "KF_USEFUL_DIRECTION_MIN_LENGTH"),
              definition(source, "    Vector3 AICar::GetUsefulDirection() const"),
              "}"]
    sys.exit(compile_and_run("AIDrvUsefulDirection.cpp", chunks, prefix="brn_aidrv_useful_"))


if __name__ == "__main__":
    main()
