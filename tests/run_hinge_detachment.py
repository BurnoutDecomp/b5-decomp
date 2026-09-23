"""Regression for the DeformableObject hinge / detachment chain (crash parity G20-D1..D8).

Run from the workflow checkout:
    python b5-decomp/tests/run_hinge_detachment.py [--pre-fix <b5 rev>]

Numeric part (HingeDetachment.cpp, shipped text extracted):
  G20-D3  UpdateSpinningDetachment integrates |w|_1 * dt, decays 0.99/frame, hinges past 8.0
  G20-D1  CheckForForcedDetachment's ordinal = draw % 11      (0x8263ADC4..0x8263AE00)
  G20-D2  UpdateSpinningDetachment's ordinal = (u8)(draw % 20) (0x8263A9BC..0x8263AA08)
  G20-D6  contact rows ordered by impact time unless within 0.1 frame (_Insertion_sort1 0x82629898)
Structural part (a missing console call/store is present):
  G20-D4  Update stores mLastAngularVelocity and leaves the spin accumulator alone
  G20-D5  Release calls DetachedWheelManager::RemoveVehicleWheels
  G20-D7  Prepare zeroes the impulse-passer map (mImpulsePasser.Prepare())
  G20-D8  ResetDeformation zeroes the 128 skinning-offset scratch rows
--pre-fix <rev> runs everything on that revision (the RED side).
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings

DIR = "src/GameSource/Physics/DeformationManager/DeformationPhysics/"


def read(name, rev):
    rel = DIR + name
    if rev:
        text = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout
    else:
        text = (REPO / rel).read_text(encoding="utf-8-sig")
    return text.replace("\r\n", "\n")


def fn_body(src, signature):
    start = src.index(signature)
    depth, i = 0, src.index("{", start)
    while True:
        c = src[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[start:i + 1]
        i += 1


def main():
    rev = sys.argv[sys.argv.index("--pre-fix") + 1] if "--pre-fix" in sys.argv else None
    detach = read("BrnDeformableObject_Detach.cpp", rev)
    update = read("BrnDeformableObject_Update.cpp", rev)
    life = read("BrnDeformableObject_Lifecycle.cpp", rev)

    spin_fn = fn_body(detach, "void DeformableObject::UpdateSpinningDetachment(")
    forced_fn = fn_body(detach, "void DeformableObject::CheckForForcedDetachment(")
    s = spin_fn.index("const f32 lfTimeStep = lvfTimeStep.x;")
    m = re.search(r"kfAngularVelocityForDetachment \) \)\s*\n\s*return;", spin_fn[s:])
    spin_inc = spin_fn[s:s + m.end()]
    draw_re = r"const u32 luPartDraw = [^;]+;"
    draws_inc = ("u32 ForcedDraw(FakeRandom* lpRandom) {\n" + re.search(draw_re, forced_fn).group(0)
                 + "\nreturn luPartDraw; }\n"
                 "u32 SpinDraw(FakeRandom& lrRandom) {\n" + re.search(draw_re, spin_fn).group(0)
                 + "\nreturn luPartDraw; }\n")
    ct0 = update.index("struct ContactTime")
    ct1 = update.index("ContactOrder _mContactOrder;", ct0) + len("ContactOrder _mContactOrder;")
    anchor = "++_mContactOrder.miNumContacts;\n        }\n"
    so0 = update.index(anchor) + len(anchor)
    so1 = update.index("// ---- [absorb] PC bring-up instrument", so0)
    order_inc = update[ct0:ct1] + "\nvoid SortRows() {\n" + update[so0:so1] + "\n}\n"

    # ---- structural checks -------------------------------------------------------------------
    failures = []
    upd_fn = fn_body(update, "bool DeformableObject::Update(") if "bool DeformableObject::Update(" in update \
        else fn_body(update, "void DeformableObject::Update(")
    if "mAngularVelocitySum = VecFloat{ lvAngular" in upd_fn or "mLastAngularVelocity =" not in upd_fn:
        failures.append("G20-D4 Update must store mLastAngularVelocity and not overwrite the spin accumulator")
    rel_fn = fn_body(life, "void DeformableObject::Release(")
    if "lpWheelMgr->RemoveVehicleWheels(" not in rel_fn:
        failures.append("G20-D5 Release must call RemoveVehicleWheels")
    prep_fn = fn_body(life, "void DeformableObject::Prepare(") if "void DeformableObject::Prepare(" in life \
        else fn_body(life, "bool DeformableObject::Prepare(")
    if "mImpulsePasser.Prepare();" not in prep_fn:
        failures.append("G20-D7 Prepare must zero the impulse-passer map")
    reset_fn = fn_body(life, "void DeformableObject::ResetDeformation(")
    if not re.search(r"maVerletOffsets_Scratch\[liRow\]\s*=\s*Vector3Plus\{", reset_fn):
        failures.append("G20-D8 ResetDeformation must zero the skinning-offset scratch rows")
    for f in failures:
        print("FAIL:", f)

    with tempfile.TemporaryDirectory(prefix="brn_hinge_") as directory:
        out = Path(directory)
        (out / "spin.inc").write_text(spin_inc, encoding="utf-8")
        (out / "draws.inc").write_text(draws_inc, encoding="utf-8")
        (out / "order.inc").write_text(order_inc, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + f' /I"{out}" "'
               + str(Path(__file__).with_name("HingeDetachment.cpp")) + '" /Fe:regression.exe /link /OPT:REF')
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + cmd + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    print(f"structural: 4 checks, {len(failures)} failures")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
