"""FX-SCENEMGR (crash parity 2026-09-24, H-SM1): InSceneUpdateInterface::ReplaceDynamicVolume (ARTIST
0x822B15F8, DWARF CgsSceneManagerIO_SceneUpdate.h:458 `void ReplaceDynamicVolume(VolumeId, const
VolRef::Volume *)`) posts the WHOLE 64-bit volume key. The tree used to carry only a fitted 32-bit
EntityId form whose body stored `(u64)(u32)id`, so a race car's mHandlingBodyVolumeId (entity word in the
high dword, low dword 0) would have replaced volume key 0.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxscenemgr_replace_dynamic_volume.py [--pre-fix <b5 rev>]
The production body is extracted from CgsSceneManagerIO_SceneUpdate.cpp (working tree or <rev>); the
header's declarations are checked structurally.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import REPO, build_and_run, code_mask, optional_definition, pre_fix_rev, read

SCENE = "src/GameShared/GameClasses/SceneManager/"
CPP = SCENE + "CgsSceneManagerIO_SceneUpdate.cpp"
HEADER = SCENE + "CgsSceneManagerIO_SceneUpdate.h"


def main():
    rev = pre_fix_rev(sys.argv)
    body = optional_definition(read(CPP, rev), "void InSceneUpdateInterface::ReplaceDynamicVolume(")
    if not body:
        print("MISSING InSceneUpdateInterface::ReplaceDynamicVolume (no body in " + CPP + ")")
        sys.exit(1)
    wide = re.match(r"void InSceneUpdateInterface::ReplaceDynamicVolume\(\s*(?:CgsSceneManager::)?VolumeId\b",
                    body) is not None
    print("found   InSceneUpdateInterface::ReplaceDynamicVolume(" + ("VolumeId" if wide else "EntityId") + ", ...)")

    header = code_mask(read(HEADER, rev))
    decl_wide = re.search(r"\bvoid\s+ReplaceDynamicVolume\(\s*(?:CgsSceneManager::)?VolumeId\b", header) is not None
    decl_narrow = re.search(r"\bvoid\s+ReplaceDynamicVolume\(\s*(?:CgsSceneManager::)?EntityId\b", header) is not None
    print(("ok      " if decl_wide else "MISSING ") + "header declaration ReplaceDynamicVolume(VolumeId, ...)")
    print(("PRESENT " if decl_narrow else "ok      ") + "header declaration ReplaceDynamicVolume(EntityId, ...)")

    pieces = {
        "fxsm_rdv_body.inc": body,
        "fxsm_rdv_form.inc": "#define FXSM_RDV_WIDE " + ("1" if wide else "0") + "\n"
                             "static const bool gbHeaderWide = " + ("true" if decl_wide else "false") + ";\n"
                             "static const bool gbHeaderNoNarrow = " + ("false" if decl_narrow else "true") + ";",
    }
    rc = build_and_run(REPO / "tests" / "FxScenemgrReplaceDynamicVolume.cpp", pieces, "fxsm_rdv",
                       extra_sources=(REPO / SCENE / "CgsVolumeInstanceId.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
