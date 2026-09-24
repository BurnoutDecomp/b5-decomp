"""FX-SCENEMGR (crash parity 2026-09-24, item 3a): PlaceOnTrackManager::PrePhysicsUpdate (ARTIST 0x822F6DF8)
reads the fine-line-test result's query id as a WORD -- owner = bits [16..23] (`extrwi 8,8` @0x822F6F78),
slot = low half (`clrlwi 16` @0x822F6F84) -- through SceneQueryId::GetOwner / GetIndex (DWARF
CgsSceneQueryId.h:57/:60). The pre-fix body read memory bytes [1] / [0], the big-endian transcription of
Hex-Rays' BYTE1, so on this host no place-on-track answer (Set(5, slot) == 0x0005000S) could match.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxscenemgr_place_on_track_query_id.py [--pre-fix <b5 rev>]
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import REPO, build_and_run, code_mask, definition, pre_fix_rev, read

MANAGER_CPP = "src/GameSource/World/BrnPlaceOnTrackManager.cpp"
MANAGER_H = "src/GameSource/World/BrnPlaceOnTrackManager.h"
QUERY_ID_H = "src/GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"


def struct_text(source, name):
    """The `struct <name> { ... };` text (brace-balanced)."""
    start = source.index("struct " + name)
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                end = source.index(";", index)
                return source[start:end + 1]
    raise ValueError("unclosed struct " + name)


def constant(source, name):
    match = re.search(r"^[^\n/]*\b" + name + r"\s*=\s*[^;]+;", source, re.M)
    if not match:
        raise ValueError("constant not found: " + name)
    return match.group(0).strip()


def main():
    rev = pre_fix_rev(sys.argv)
    cpp = read(MANAGER_CPP, rev)
    header = read(MANAGER_H, rev)
    query_id = struct_text(read(QUERY_ID_H, rev), "SceneQueryId")
    accessors = re.search(r"\bGetOwner\s*\(", code_mask(query_id)) is not None
    print(("found   " if accessors else "MISSING ") + "SceneQueryId::Set / GetOwner / GetIndex")

    body = definition(cpp, "void PlaceOnTrackManager::PrePhysicsUpdate(")
    consts = "\n".join([constant(cpp, "KI_OUT_EVENT_LINE_TEST_FINE_RESULT"),
                        constant(header, "KI_PLACE_ON_TRACK_LINE_TEST_OWNER")])
    pieces = {
        "fxsm_qid_struct.inc": query_id,
        "fxsm_qid_form.inc": "#define FXSM_QID_ACCESSORS " + ("1" if accessors else "0"),
        "fxsm_qid_consts.inc": consts,
        "fxsm_qid_prephysics.inc": body,
    }
    rc = build_and_run(REPO / "tests" / "FxScenemgrPlaceOnTrackQueryId.cpp", pieces, "fxsm_qid")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
