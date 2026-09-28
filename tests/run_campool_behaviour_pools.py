"""L1 CAMPOOL (owner's list 2026-09-27, "There are still some asserts and crashes"): the camera-behaviour pools.

  The owner's repeated crash (three WER dumps on exe d65db9997047): "Ran out of slots when trying to allocate a large
  behaviour" (BrnBehaviourManager.h), then an access violation in ObjectPool<Vector4[250],8,int>::operator[] under
  NewBehaviour<BehaviourGyroCam> <- B3ClassicTakedownPlayer::Prepare <- ArbStateTakedown::Prepare. The console's twenty
  AllocateBehaviour<> siblings each name ONE pool in their asm (@0x82263370 and siblings; the LARGE ones read the free
  count at +0x7D30 and assert h:1148, the SMALL ones read +0xFAA0 and assert h:1137). Only Failsafe, GameplayBumper,
  GameplayExternal and IceAnim are LARGE. The PC chose the pool from the HOST sizeof against the console's 1600-byte
  small bucket, and BehaviourGyroCam -- 0x640 == 1600 bytes on the console (`li r7,0x640` @0x822598D8) -- is 1632 on
  x64, so every gyro rig went to the 8-slot large pool. The dumps hold four GyroCams there next to the two gameplay
  cameras and two ICE takes; the fifth found it empty. The console's small pool had 15 of 20 free.

Numeric: tests/CampoolBehaviourPools.cpp runs the revision's own AllocateBehaviour<> body (with its pool table, pool
constants/typedefs and pool member declarations, extracted as text from BrnBehaviourManager.h) on fixture behaviours
that carry the revision's REAL host size and alignment of each type (probed by compiling the real behaviour headers).
Per type: the pool it lands in vs the console's sibling, and that it fits its slot. Then the owner's crash set from the
dumps, which on the old body asserts exactly as the owner's game did.
Wiring: a host type larger than its console pool's bucket must NOT compile (it would be placement-new'd over the next
slot), and BrnBehaviourManager.cpp pins the host small bucket to the host BehaviourGyroCam.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_campool_behaviour_pools.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import WORKFLOW, Tree, code_only, compile_and_run, definition, report, settings

MANAGER_H = "src/GameSource/Director/Camera/BrnBehaviourManager.h"
MANAGER_CPP = "src/GameSource/Director/Camera/BrnBehaviourManager.cpp"
ABSTRACT_POOL_CPP = "src/GameSource/Director/Utils/BrnAbstractPool.cpp"
NUMERIC_CHECKS = 45   # 20 types x (pool + fit) + 5 crash-set checks

# The twenty console types: (name, class-key, probe group). The group TU is BrnBehaviourManager.cpp's own include set;
# IceAnim / RenderMetrics / Rig collide with it and are probed alone, as the tree instantiates them alone.
GROUP_HEADERS = (
    "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h",
    "GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h",
    "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h",
    "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h",
)
GROUP_TYPES = ("AftertouchCam", "AftertouchCrash", "BystanderCam", "DebugFlyWorld", "DebugOrbitPlayer", "Failsafe",
               "FixedCam", "GameplayBumper", "GameplayExternal", "GyroCam", "HeliCam", "Interpolate",
               "LooseAttachment", "PassengerCam", "RoadRunner", "RotateAboutVehicle", "SpirallingDeathcam")
ALONE = (("IceAnim", "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"),
         ("RenderMetrics", "GameSource/Director/Camera/Behaviours/BrnBehaviourRenderMetrics.h"),
         ("Rig", "GameSource/Director/Camera/Behaviours/BehaviourRig.h"))
STRUCT_TYPES = {"PassengerCam"}


def run_cl(sources, shadow_dir, work, extra_flags="", link=True):
    """Compile (and optionally link + run) with the canonical flags; returns (returncode, stdout)."""
    includes = (f'/I"{shadow_dir}" ' if shadow_dir else "") + f'/I"{work}" ' + " ".join(
        f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
    command = ("cl " + " ".join(settings("msvc_flags.txt")) + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public "
               "/Dprotected=public " + extra_flags + " " + includes + "".join(f' "{s}"' for s in sources))
    command += " /Fe:probe.exe /link /OPT:REF" if link else " /c"
    # NOT "cl.cmd": msvc_env.bat probes `cl 2>&1`, and with the current directory searchable that name would resolve to
    # this script and recurse without end.
    script = work / "campool_build.cmd"
    body = ('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n'
            + command + "\nif errorlevel 1 exit /b 90\n")
    if link:
        body += '"%~dp0probe.exe"\nexit /b %ERRORLEVEL%\n'
    script.write_text(body, encoding="utf-8", newline="\r\n")
    result = subprocess.run(["cmd", "/c", str(script)], cwd=work, capture_output=True, text=True, errors="replace")
    return result.returncode, result.stdout + result.stderr


def shadow_manager(tree, root):
    """Write the revision's manager header under <root>/shadow so every compile sees the revision's text."""
    shadow = root / "shadow"
    target = shadow / Path(MANAGER_H).relative_to("src")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(tree.read(MANAGER_H), encoding="utf-8")
    return shadow


def probe_sizes(tree, root):
    """The revision's host sizeof / alignof of every console behaviour type, from its REAL headers."""
    shadow = shadow_manager(tree, root)
    sizes = {}
    jobs = [("group", GROUP_HEADERS, GROUP_TYPES)] + [(name, (header,), (name,)) for name, header in ALONE]
    for tag, headers, types in jobs:
        work = root / f"probe_{tag}"
        work.mkdir()
        source = work / f"campool_probe_{tag}.cpp"
        lines = ['#include "GameSource/Director/Camera/BrnBehaviourManager.h"']
        lines += [f'#include "{header}"' for header in headers]
        lines += ["#include <cstdio>", "int main()", "{"]
        lines += [f'    std::printf("CAMPOOL_SIZE {name} %zu %zu\\n", sizeof(BrnDirector::Camera::Behaviour{name}), '
                  f"alignof(BrnDirector::Camera::Behaviour{name}));" for name in types]
        lines += ["    return 0;", "}"]
        source.write_text("\n".join(lines) + "\n", encoding="utf-8")
        code, output = run_cl([source], shadow, work)
        for match in re.finditer(r"CAMPOOL_SIZE (\w+) (\d+) (\d+)", output):
            sizes[match.group(1)] = (int(match.group(2)), int(match.group(3)))
        if code != 0:
            print(f"NUMERIC: the size probe '{tag}' failed (exit {code})\n" + output[-3000:])
            return None
    return sizes


def extract_sandbox(tree):
    """The revision's pool table / constants / typedefs / members / AllocateBehaviour body, as text."""
    source = tree.read(MANAGER_H)
    table = ""
    start = source.find("enum EBehaviourPool")
    if start >= 0:
        last = source.rfind("template <> struct ConsoleBehaviourPool<")
        table = source[start:source.index("\n", last)] + "\n"
    constants = "\n".join(m.group(0) for m in re.finditer(
        r"static const u32 KU_\w*BEHAVIOUR_POOL\w*\s*=\s*\d+u\s*;", source))
    typedefs = "\n".join(m.group(0) for m in re.finditer(
        r"typedef\s+BrnDirector::AbstractPool<[^;]*>\s*(?:Large|Small)BehaviourPool\s*;", source))
    members = "\n".join(m.group(0) for m in re.finditer(
        r"^[ \t]*[\w:<>, ]+?\s+m(?:Large|Small)BehaviourPool\s*;", source, re.M))
    try:
        body = definition(source, "inline AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour()")
    except ValueError:
        return None
    return table, constants, typedefs, members, "template <typename TBehaviour>\n" + body


def sandbox_text(sizes, extracted, oversize=None):
    table, constants, typedefs, members, body = extracted
    out = ['#include "GameSource/Director/Utils/BrnAbstractPool.h"',
           '#include "GameShared/GameClasses/Core/CgsAssert.h"',
           '#include "rw/math/vpu/types.h"',
           "namespace BrnDirector", "{", "namespace Camera", "{"]
    for name in GROUP_TYPES + tuple(n for n, _ in ALONE):
        size, align = sizes[name]
        if oversize and oversize[0] == name:
            size += oversize[1]
        key = "struct" if name in STRUCT_TYPES else "class"
        out.append(f"    {key} Behaviour{name} {{ public: alignas({align}) unsigned char maBytes[{size}]; }};")
    out += [table,
            "    class BehaviourManager", "    {", "    public:", constants, typedefs,
            "        template <typename TBehaviour> AbstractPoolVoidHandle AllocateBehaviour();",
            "        void DebugDumpToTTY() const;", members, "    };", body, "}", "}"]
    return "\n".join(out) + "\n"


def numeric(tree, sizes, extracted):
    if sizes is None or extracted is None:
        return None
    return compile_and_run(Path(__file__).with_name("CampoolBehaviourPools.cpp"), "campool_sandbox.inc",
                           sandbox_text(sizes, extracted), "CampoolBehaviourPools",
                           extra_sources=(tree_file(tree, ABSTRACT_POOL_CPP),))


def tree_file(tree, relative):
    """A path holding the revision's text of one source file (the working tree's own path when rev is None)."""
    if tree.rev is None:
        return Path(__file__).resolve().parents[1] / relative
    target = Path(tempfile.mkdtemp(prefix="brn_campool_src_")) / Path(relative).name
    target.write_text(tree.read(relative), encoding="utf-8")
    return target


def rejects(sizes, extracted, oversize, root, phrase):
    """True when a sandbox whose `oversize` type outgrows its console bucket FAILS to compile on `phrase`."""
    if sizes is None or extracted is None:
        return False
    work = root / f"reject_{oversize[0]}"
    work.mkdir()
    (work / "campool_sandbox.inc").write_text(sandbox_text(sizes, extracted, oversize), encoding="utf-8")
    driver = work / "campool_reject.cpp"
    driver.write_text('#include "campool_sandbox.inc"\n'
                      "namespace CgsDev { namespace Assert { int BeginAssert() { return 0; } "
                      "int FireAssert(const char*, const char*, int) { return 0; } void* EndAssert() { return 0; } } }\n"
                      "void BrnDirector::Camera::BehaviourManager::DebugDumpToTTY() const {}\n"
                      "BrnDirector::AbstractPoolVoidHandle Campool(BrnDirector::Camera::BehaviourManager& m)\n"
                      f"{{ return m.AllocateBehaviour<BrnDirector::Camera::Behaviour{oversize[0]}>(); }}\n",
                      encoding="utf-8")
    code, output = run_cl([driver], None, work, link=False)
    return code != 0 and phrase in output


def wiring(tree, sizes, extracted, root):
    # 16 bytes past the host small bucket: the console gives GyroCam the small pool, so this must not compile.
    yield ("a GyroCam one Vector4 larger than the host small bucket does not compile (no slot overrun)",
           rejects(sizes, extracted, ("GyroCam", 16), root, "small bucket"))
    # 32 bytes past the 4000-byte large bucket for the largest large behaviour.
    if sizes is not None:
        grow = 4000 - sizes["IceAnim"][0] + 16
        yield (f"an IceAnim grown past the 4000-byte large bucket (+{grow}) does not compile (no slot overrun)",
               rejects(sizes, extracted, ("IceAnim", grow), root, "large bucket"))
    cpp = code_only(tree.read(MANAGER_CPP))
    yield ("BrnBehaviourManager.cpp pins the host small bucket to the host BehaviourGyroCam (static_assert)",
           re.search(r"static_assert\s*\(\s*sizeof\s*\(\s*BehaviourManager::SmallBehaviourPool::Bucket\s*\)"
                     r"[\s\S]{0,200}sizeof\s*\(\s*BehaviourGyroCam\s*\)", cpp) is not None)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    with tempfile.TemporaryDirectory(prefix="brn_campool_") as directory:
        root = Path(directory)
        sizes = probe_sizes(tree, root)
        if sizes is not None:
            print("host sizes: " + ", ".join(f"{name} {size}" for name, (size, _) in sorted(sizes.items())))
        extracted = extract_sandbox(tree)
        if extracted is None:
            print("NUMERIC: the revision has no BehaviourManager::AllocateBehaviour body")
        checks = list(wiring(tree, sizes, extracted, root))
        return report("run_campool_behaviour_pools", checks, numeric(tree, sizes, extracted), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
