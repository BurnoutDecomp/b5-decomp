"""FX-CRASHSND item 2 (crash parity 2026-09-24): the collision-audio bin lookup caches.

ARTIST CollisionStateManager::ResourcesAreReady @0x826D3788 builds two BinLookupCaches -- the
crash-bin list's (0x826D37C0, Build<crashbinlist, crashbin> @0x826A85F8) and the props crash-bin
list's (0x826D37D0, Build<propscrashbinlist, propscrashbin> @0x826A8710) -- raises
mbResourcesAreLoaded on every call (0x826D37E0) and constructs NO Content: Prepare @0x826F8B78
state 2 constructs the three Contents once that flag is up (0x826F8C64 / 0x826F8CA0 / 0x826F8CDC)
and registers the "Collisions" CPU monitor (0x826F8E1C). SelectBin<List, Bin> (@0x826A97E8 /
@0x826A8828) walks maBinLoopupCache[mePipeline] for the material pre-filter. On the PC nothing
called Build, BrnBinLookupCache.cpp had no mount line and no crashbin instantiation, the Contents
were constructed in ResourcesAreReady, and the selection read the materials off a fresh Bin.

Wiring: the lifecycle glue and the mount. Numeric: tests/FxCrashSndBinCache.cpp compiles the
PRODUCTION BinLookupCache class, Build, and SelectCollisionBin's cache walk against a fixture
AttribSys. --rev reads the b5 sources at that revision and the PARENT's committed build script
(the mount is a parent-repo line; the working-tree run reads the working file).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashsnd_bin_cache.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, WORKFLOW, definition, code_only, body_or_empty, compile_and_run, report

CACHE_H = "src/GameSource/Sound/Collision/BrnBinLookupCache.h"
CACHE_CPP = "src/GameSource/Sound/Collision/BrnBinLookupCache.cpp"
MANAGER_CPP = "src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp"
MANAGER_H = "src/GameSource/Sound/Collision/BrnCollisionStateManager.h"
CRASHBIN_H = "src/GameSource/AttribSys/Generated/classes/crashbin.h"
PROPSCRASHBIN_H = "src/GameSource/AttribSys/Generated/classes/propscrashbin.h"
BUILD_BAT = "tools/build/build_game_exe.bat"

RESOURCES_ARE_READY = "void CollisionStateManager::ResourcesAreReady()"
PREPARE = "bool CollisionStateManager::Prepare()"
SELECT = "void CollisionStateManager::SelectCollisionBin("
BUILD = "void BinLookupCache::Build( const List& lrList )"
NUMERIC_CHECKS = 28


def parent_build_script(rev):
    path = WORKFLOW / BUILD_BAT
    if rev is None:
        return path.read_text(encoding="utf-8", errors="replace")
    result = subprocess.run(["git", "-C", str(WORKFLOW), "show", "HEAD:" + BUILD_BAT],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    return result.stdout if result.returncode == 0 else ""


def if_block(code, condition):
    """The brace-balanced block of the first `if (<condition>)` in code, or ''."""
    match = re.search(r"if\s*\(\s*" + condition + r"\s*\)\s*\{", code)
    if not match:
        return ""
    depth = 0
    for index in range(match.end() - 1, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return code[match.start():index + 1]
    return ""


def updating_arm(prepare_code):
    match = re.search(r"case\s+E_PREPARE_UPDATING\s*:(.*?)case\s+E_PREPARE_STATES\s*:", prepare_code, re.S)
    return match.group(1) if match else ""


def select_head(select_code):
    """SelectCollisionBin from the cache binding through the material-hit counter."""
    match = re.search(r"(const\s+BinLookupCache\s*&\s*lrCache.*?\+\+luMaterialBins\s*;)", select_code, re.S)
    return match.group(1) if match else ""


def wiring(tree, rev):
    ready = body_or_empty(tree.read(MANAGER_CPP), RESOURCES_ARE_READY)
    loaded_block = if_block(ready, r"!\s*mbResourcesAreLoaded")
    after_block = ready[ready.find(loaded_block) + len(loaded_block):] if loaded_block else ""
    prepare = body_or_empty(tree.read(MANAGER_CPP), PREPARE)
    arm = updating_arm(prepare)
    gate = re.search(r"if\s*\(\s*!\s*mbResourcesAreLoaded\s*\)\s*return\s+false\s*;", arm)
    constructs_after_gate = arm[gate.end():] if gate else ""
    select = body_or_empty(tree.read(MANAGER_CPP), SELECT)
    manager_h = code_only(tree.read(MANAGER_H))
    cache_cpp = code_only(tree.read(CACHE_CPP))
    crashbin = code_only(tree.read(CRASHBIN_H))
    propscrashbin = code_only(tree.read(PROPSCRASHBIN_H))
    mount = re.search(r'echo\s+"%SRC%\\GameSource\\Sound\\Collision\\BrnBinLookupCache\.cpp"',
                      parent_build_script(rev))
    return [
        ("CollisionStateManager holds BinLookupCache maBinLoopupCache[E_MAX_PIPELINES] (DWARF h:787)",
         re.search(r"BinLookupCache\s+maBinLoopupCache\s*\[\s*InputCollision::E_MAX_PIPELINES\s*\]\s*;",
                   manager_h) is not None),
        ("ResourcesAreReady builds maBinLoopupCache[E_REGULAR] from mCrashBinList while !mbResourcesAreLoaded "
         "(0x826D37C0)",
         re.search(r"maBinLoopupCache\s*\[\s*InputCollision::E_REGULAR\s*\]\s*\.\s*Build\s*<\s*Attrib::Gen::"
                   r"crashbinlist\s*,\s*Attrib::Gen::crashbin\s*>\s*\(\s*mCrashBinList\s*\)", loaded_block)
         is not None),
        ("ResourcesAreReady builds maBinLoopupCache[E_PROP] from mPropsCrashBinList (0x826D37D0)",
         re.search(r"maBinLoopupCache\s*\[\s*InputCollision::E_PROP\s*\]\s*\.\s*Build\s*<\s*Attrib::Gen::"
                   r"propscrashbinlist\s*,\s*Attrib::Gen::propscrashbin\s*>\s*\(\s*mPropsCrashBinList\s*\)",
                   loaded_block) is not None),
        ("ResourcesAreReady raises mbResourcesAreLoaded after the build block, on every call (0x826D37E0)",
         "mbResourcesAreLoaded = true" not in loaded_block and
         re.search(r"mbResourcesAreLoaded\s*=\s*true\s*;", after_block) is not None),
        ("ResourcesAreReady constructs no Content (the console does that in Prepare)",
         ready != "" and ".Construct(" not in ready),
        ("Prepare state 2 waits for mbResourcesAreLoaded, then constructs ScrapesCsis / ScrapePatchBank.abi on "
         "the AEMS factory and the splice bank on the splicer factory, each only if not created "
         "(0x826F8C44..0x826F8D00)",
         gate is not None and
         re.search(r"if\s*\(\s*!\s*mScrapesCsisInterface\s*\.\s*IsCreated\(\)\s*\)\s*mScrapesCsisInterface\s*"
                   r"\.\s*Construct\([^;]*luAemsFactory[^;]*\"ScrapesCsis\"", constructs_after_gate) is not None and
         re.search(r"if\s*\(\s*!\s*mScrapesAemsBank\s*\.\s*IsCreated\(\)\s*\)\s*mScrapesAemsBank\s*\.\s*"
                   r"Construct\([^;]*luAemsFactory[^;]*\"ScrapePatchBank\.abi\"", constructs_after_gate)
         is not None and
         re.search(r"if\s*\(\s*!\s*mCollisionSplicerBank\s*\[\s*E_COLLISION_SPLICE_BANK_COLLISION\s*\]\s*\.\s*"
                   r"IsCreated\(\)\s*\)\s*mCollisionSplicerBank\s*\[\s*E_COLLISION_SPLICE_BANK_COLLISION\s*\]\s*"
                   r"\.\s*Construct\([^;]*\"~SplicerFactory::SK_NAME~\"[^;]*\"CollisionSpliceBank\"",
                   constructs_after_gate) is not None and
         "AemsFactorySkName()" in arm),
        ("Prepare state 2 registers the \"Collisions\" CPU monitor (0x826F8E00..0x826F8E20)",
         re.search(r"miCpuMonitor\s*=\s*CgsDev::PerfMonCpu::AddMonitor\(\s*\"Collisions\"\s*,\s*14\s*,\s*0\s*,"
                   r"\s*1\.0f?\s*,\s*0\s*,\s*1\s*\)", arm) is not None),
        ("BrnBinLookupCache.cpp instantiates Build<crashbinlist, crashbin> (@0x826A85F8) and "
         "Build<propscrashbinlist, propscrashbin> (@0x826A8710)",
         re.search(r"template\s+void\s+BinLookupCache::Build\s*<\s*Attrib::Gen::crashbinlist\s*,\s*"
                   r"Attrib::Gen::crashbin\s*>", cache_cpp) is not None and
         re.search(r"template\s+void\s+BinLookupCache::Build\s*<\s*Attrib::Gen::propscrashbinlist\s*,\s*"
                   r"Attrib::Gen::propscrashbin\s*>", cache_cpp) is not None),
        ("SelectCollisionBin walks maBinLoopupCache[mePipeline] with GetEntry (0x826A987C / 0x826A9A6C)",
         re.search(r"maBinLoopupCache\s*\[\s*lrOutput\s*\.\s*mePipeline\s*\]", select) is not None and
         ".GetEntry(" in select),
        ("the image's Bin constants: crashbin ClassKey 0x3DFA53FA_FE5BD9D7, propscrashbin 0x4154BD6D_E9FF326C, "
         "both 0x190 / +0x40 A / +0x38 B",
         re.search(r"KU_CLASS_KEY\s*=\s*0x3DFA53FAFE5BD9D7ull", crashbin) is not None and
         re.search(r"static\s+u64\s+ClassKey\(\)\s*\{\s*return\s+KU_CLASS_KEY\s*;\s*\}", crashbin) is not None and
         re.search(r"KU_OFFSET_MATERIAL_A\s*=\s*0x40u", crashbin) is not None and
         re.search(r"KU_OFFSET_MATERIAL_B\s*=\s*0x38u", crashbin) is not None and
         "0x4154BD6DE9FF326CULL" in propscrashbin and
         re.search(r"KU_OFFSET_MATERIAL_A\s*=\s*0x40u", propscrashbin) is not None),
        ("the parent mounts BrnBinLookupCache.cpp (tools/build/build_game_exe.bat)", mount is not None),
    ]


def numeric(tree):
    header = tree.read(CACHE_H)
    try:
        klass = definition(header, "class BinLookupCache") + ";"
        build = "template< typename List, typename Bin >\n" + definition(tree.read(CACHE_CPP), BUILD)
        select = code_only(definition(tree.read(MANAGER_CPP), SELECT))
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    head = select_head(select)
    if not head:
        print("NUMERIC: cannot build -- SelectCollisionBin has no BinLookupCache walk")
        return None
    return compile_and_run(Path(__file__).with_name("FxCrashSndBinCache.cpp"), "fxcrashsnd_bincache_class.inc",
                           klass, "FxCrashSndBinCache",
                           extra_files={"fxcrashsnd_bincache_build.inc": build,
                                        "fxcrashsnd_select_head.inc": head})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxcrashsnd_bin_cache", wiring(tree, args.rev), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
