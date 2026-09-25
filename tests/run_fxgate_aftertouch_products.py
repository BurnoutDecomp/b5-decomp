"""FX-GATE (crash parity 2026-09-25): BehaviourAftertouchCrash::Update's three rotation products are the console's fused
cascades -- Utils::ConsoleVpu (GameSource/Director/Camera/Utils/BrnConsoleVpu.h, FX-DIRECTOR2 2c59d16d) -- not the
unfused vendor rw::math::vpu forms. Supersedes b6a44002's "rule 3, open" FLAGs.

  orbit  0x8222875C..0x82228764  TransformVector(XMMatrixRotationY(stick x * -0.05), direction): row0 * x (vmulfp128),
                                 + row1 * y, + row2 * z (vmaddfp, ROUNDING_RULE 3)
  pitch  0x822290C0..0x82229144  Mult(RotationX(mfPitch * 0.017453292), the CreateLookAt frame, wAxis.y += mfHeight)
  roll   0x8222944C..0x822294BC  Mult(RotationZ(mfRollAngleRads), the camera's transform)

  1. WIRING -- the three statements call Utils::ConsoleVpu::TransformVector / Mult; the file includes BrnConsoleVpu.h.
  2. NUMERIC -- the three statements are EXTRACTED (with the file-local rotation builders and constants) and compared
     bit for bit with tests/FxGateAftertouchProductsData.h: 120 rows per site, each the WHOLE block of the console's
     words run on emu64 from its inputs (scratch/CRASHPARITY_0922/fixes/FX-GATE.sincos/gen_products_data.py), plus
     KF_CAMERA_X_ROTATION_SPEED against the image (0xBD4CCCCD @0x82CDA718).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_aftertouch_products.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

ATC = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.cpp"
UPDATE = "bool BehaviourAftertouchCrash::Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo)"
BUILDERS = ["Matrix44Affine RotationX(f32 lfAngleRads)", "Matrix44Affine RotationZ(f32 lfAngleRads)",
            "Matrix44Affine XMMatrixRotationY(f32 lfAngleRads)"]
EXPECT = [
    ("orbit TransformVector 0x8222875C", "orbit",
     "mManualCameraDirection=Utils::ConsoleVpu::TransformVector(XMMatrixRotationY(lOrbitStick.x*KF_CAMERA_X_ROTATION_SPEED),"
     "mManualCameraDirection);"),
    ("pitch Mult 0x822290C0", "pitch",
     "lCameraTransform=Utils::ConsoleVpu::Mult(RotationX(lrParameters.mfPitch*KF_DEGS_TO_RADS),lCameraTransform);"),
    ("roll Mult 0x8222944C", "roll",
     "lrCamera.SetTransform(Utils::ConsoleVpu::Mult(RotationZ(mfRollAngleRads),lrCamera.GetTransform()));"),
]
NUMERIC_CHECKS = 361


def statement_around(body, marker, head):
    """The statement that contains `marker`, starting at the last `head` before it."""
    m = body.find(marker)
    if m < 0:
        return None
    s = body.rfind(head, 0, m)
    e = body.find(";", m)
    if s < 0 or e < 0:
        return None
    return body[s:e + 1]


def statements(tree):
    try:
        body = code_only(definition(tree.read(ATC).replace("\r\n", "\n"), UPDATE))
    except ValueError:
        return None
    out = {"orbit": statement_around(body, "lOrbitStick.x * KF_CAMERA_X_ROTATION_SPEED", "mManualCameraDirection ="),
           "roll": statement_around(body, "RotationZ(mfRollAngleRads)", "lrCamera.SetTransform(")}
    lift = "lCameraTransform.wAxis.y += mfHeight;"
    s = body.find(lift)
    if s >= 0:
        e = body.find(";", body.find("lCameraTransform =", s))
        out["pitch"] = body[s:e + 1] if e >= 0 else None
    else:
        out["pitch"] = None
    return out


def wiring(tree):
    found = statements(tree) or {}
    for label, key, text in EXPECT:
        stmt = found.get(key)
        yield (f"{label}: {text}", stmt is not None and text in re.sub(r"\s+", "", stmt))
    yield ("BrnBehaviourAftertouchCrash.cpp includes Utils/BrnConsoleVpu.h",
           '#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"' in tree.read(ATC))


def numeric(tree):
    source = tree.read(ATC).replace("\r\n", "\n")
    parts = []
    degs = re.search(r"\bconst\s+f32\s+KF_DEGS_TO_RADS\s*=\s*([^;]+);", code_only(source))
    speed = re.search(r"\bf32\s+BehaviourAftertouchCrash::KF_CAMERA_X_ROTATION_SPEED\s*=\s*([^;]+);", code_only(source))
    if degs is None or speed is None:
        print("NUMERIC: cannot build -- KF_DEGS_TO_RADS / KF_CAMERA_X_ROTATION_SPEED not found")
        return None
    parts.append(f"const f32 KF_DEGS_TO_RADS = {degs.group(1).strip()};")
    parts.append(f"const f32 KF_CAMERA_X_ROTATION_SPEED = {speed.group(1).strip()};")
    for signature in BUILDERS:
        try:
            parts.append(definition(source, signature))
        except ValueError:
            print(f"NUMERIC: cannot build -- `{signature}` is not in the source")
            return None
    found = statements(tree)
    if not found or any(found.get(k) is None for k in ("orbit", "pitch", "roll")):
        print("NUMERIC: cannot build -- a product statement was not found")
        return None
    kernels = (
        "static Vector3 K_Orbit(f32 lfStickX, const Vector3& lrDirection)\n{\n"
        "    struct StickVector { f32 x, y; } lOrbitStick = { lfStickX, 0.0f };\n"
        "    Vector3 mManualCameraDirection = lrDirection;\n"
        f"    {found['orbit']}\n    return mManualCameraDirection;\n}}\n\n"
        "static Matrix44Affine K_Pitch(f32 lfPitchDegrees, f32 lfHeight, const Matrix44Affine& lrFrame)\n{\n"
        "    struct PitchParameters { f32 mfPitch; } lrParameters = { lfPitchDegrees };\n"
        "    const f32 mfHeight = lfHeight;\n"
        "    Matrix44Affine lCameraTransform = lrFrame;\n"
        f"    {found['pitch']}\n    return lCameraTransform;\n}}\n\n"
        "static Matrix44Affine K_Roll(f32 lfRoll, const Matrix44Affine& lrTransform)\n{\n"
        "    struct FakeCamera\n    {\n        Matrix44Affine mTransform;\n"
        "        void SetTransform(const Matrix44Affine& lrValue) { mTransform = lrValue; }\n"
        "        const Matrix44Affine& GetTransform() const { return mTransform; }\n    } lrCamera;\n"
        "    lrCamera.mTransform = lrTransform;\n"
        "    const f32 mfRollAngleRads = lfRoll;\n"
        f"    {found['roll']}\n    return lrCamera.mTransform;\n}}\n")
    return compile_and_run(Path(__file__).with_name("FxGateAftertouchProducts.cpp"),
                           "fxgate_aftertouch_products_builders.inc", "\n\n".join(parts), "FxGateAftertouchProducts",
                           extra_files={"fxgate_aftertouch_products_kernels.inc": kernels})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_aftertouch_products", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
