"""Regression for VehicleInputInterface::Construct @0x822E66A0 (crash parity G27-D1; prepared by
FX-DEFORM-LAT, landed by FX-XLANE -- the source lives in src/GameSource/Physics/VehicleManager/SharedIO/).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_vehicle_input_construct.py [--pre-fix <b5 rev>] [--src <dir>]
--src <dir> reads BrnVehicleInputInterface.cpp from <dir> instead of the tree.
The pre-fix body (b5 b636fdc6) fails 1/4 checks (the 0x822E6754 store); the fix passes 4/4.
"""
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fxdeformlat_common import build_and_run, definition, pre_fix_rev, read

TU = "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    if "--src" in sys.argv:
        src = Path(sys.argv[sys.argv.index("--src") + 1])
        text = (src / "BrnVehicleInputInterface.cpp").read_text(encoding="utf-8-sig").replace("\r\n", "\n")
    else:
        text = read(TU, rev)
    inc = "\n".join(["namespace BrnPhysics { namespace Vehicle {",
                     definition(text, "    void VehicleInputInterface::Construct()"),
                     "} }"])
    rc = build_and_run(Path(__file__).with_name("FxDeformLatVehicleInputConstruct.cpp"), {"methods.inc": inc},
                       "fxdeformlat_vehicle_input_construct", open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
