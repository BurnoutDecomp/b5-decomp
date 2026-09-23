"""Regression for crash-parity G62-D1 / G62-D2 (FX-RCEM3, 2026-09-23): the production
ActiveRaceCar::RenderParams::SetWheelScale (ARTIST 0x822CD170) and RenderParams::Reset
(ARTIST 0x822E6818), extracted from BrnActiveRaceCarRenderParams.cpp with the file's own
PIN_RP_OFFSETS layout pin and run against the real RenderParams layout (FxRcem3RenderParams.cpp).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem3_render_params.py [--pre-fix <b5 rev>]

  G62-D1: wAxis.w of the built scale matrix is the console's integer zero (sp+0x8C), not 1.0f.
  G62-D2: Reset leaves the 128 verlet rows (+0x40..+0x83F) untouched -- no console store there.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read

SOURCE = RCEM + "BrnActiveRaceCarRenderParams.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    text = read(SOURCE, rev)
    pin = re.search(r"^#define PIN_RP_OFFSETS\(\)[\s\S]*?\} while \(0\)\n", text, re.M)
    if not pin:
        print("FAIL: PIN_RP_OFFSETS macro not found")
        sys.exit(1)
    pieces = [pin.group(0), "namespace BrnWorld {",
              definition(text, "void ActiveRaceCar::RenderParams::SetWheelScale("),
              definition(text, "void ActiveRaceCar::RenderParams::Reset()"),
              "}  // namespace BrnWorld"]
    rc = build_and_run(REPO / "tests" / "FxRcem3RenderParams.cpp",
                       {"fxrcem3_render_params.inc": "\n".join(pieces)}, "fxrcem3_render_params",
                       extra_flags="/D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
