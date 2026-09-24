"""FX-SCENEMGR (crash parity 2026-09-24, item 3b): the scene manager's fine line test --
SceneManagerModule::ProcessLineTestFine (ARTIST 0x828CDCD0, an assert-false stub before),
ProcessTriangleCollisionLineTests (0x828C6FB0, a trap on a non-empty queue before),
OutSceneQueryResultsQueue::AddLineTestFineResult (0x828C4A08, spelled AllocateLineTestFineResult before)
and AddTriangleCollisionLineTestResult (0x828C4A60, absent), and the OutEventLineTestFineResult record
(DWARF CgsSceneManagerModuleIO.h:217 -- records at +0x10; the tree had them at +0x38).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxscenemgr_line_test_fine.py [--pre-fix <b5 rev>]
A body absent from the revision is replayed as a stand-in that posts nothing.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import REPO, build_and_run, code_mask, definition, optional_definition, pre_fix_rev, read

SCENE = "src/GameShared/GameClasses/SceneManager/"
MODULE = SCENE + "CgsSceneManagerModule.cpp"
QUEUE_H = SCENE + "CgsSceneManagerIO_SceneQueryResultsQueue.h"
RESULT_HPP = SCENE + "CgsSceneManagerIO_LineTestFineResult.hpp"

QUEUE_STANDINS = """
template <s32 SizeBytes> LineTestIntersection* OutSceneQueryResultsQueue<SizeBytes>::AddLineTestFineResult(SceneQueryId, s32) { return nullptr; }
template <s32 SizeBytes> LineTestIntersection* OutSceneQueryResultsQueue<SizeBytes>::AddTriangleCollisionLineTestResult(
    SceneQueryId, EntityId, VolumeInstanceId, const CgsGeometric::PolySoupLineNearestResult*, s32) { return nullptr; }
"""


def namespace_block(source, name):
    """The contents of the first `namespace <name> { ... }` block."""
    match = re.search(r"namespace\s+" + name + r"\s*\{", source)
    if not match:
        raise ValueError("namespace not found: " + name)
    depth, start = 0, match.end() - 1
    masked = code_mask(source)
    for index in range(start, len(masked)):
        if masked[index] == "{":
            depth += 1
        elif masked[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start + 1:index]
    raise ValueError("unclosed namespace " + name)


def constant(source, name, fallback):
    match = re.search(r"^[^\n/]*\b" + name + r"\s*=\s*[^;]+;", code_mask(source), re.M)
    if not match:
        print("MISSING constant " + name)
        return fallback
    return source[match.start():match.end()].strip()


def main():
    rev = pre_fix_rev(sys.argv)
    module = read(MODULE, rev)
    queue = read(QUEUE_H, rev)
    result = read(RESULT_HPP, rev)

    fine = definition(module, "void SceneManagerModule::ProcessLineTestFine(")
    tri = definition(module, "void SceneManagerModule::ProcessTriangleCollisionLineTests(")
    add_fine = optional_definition(queue, "LineTestIntersection* OutSceneQueryResultsQueue<SizeBytes>::AddLineTestFineResult(")
    add_tri = optional_definition(queue, "LineTestIntersection* OutSceneQueryResultsQueue<SizeBytes>::AddTriangleCollisionLineTestResult(")
    print(("found   " if add_fine else "MISSING ") + "OutSceneQueryResultsQueue::AddLineTestFineResult")
    print(("found   " if add_tri else "MISSING ") + "OutSceneQueryResultsQueue::AddTriangleCollisionLineTestResult")
    if add_fine and add_tri:
        bodies = "template <s32 SizeBytes>\n" + add_fine + "\n\ntemplate <s32 SizeBytes>\n" + add_tri + "\n"
    else:
        bodies = QUEUE_STANDINS

    result_ns = namespace_block(result, "SceneManagerIO")
    records_at_16 = re.search(r"\bGetIntersections\s*\(", code_mask(result_ns)) is not None
    print(("ok      " if records_at_16 else "MISSING ") + "OutEventLineTestFineResult::GetIntersections (records at +0x10)")

    consts = "\n".join([
        constant(module, "KU16_MAX_WORLD_LINE_TEST_RESULTS", "const u16 KU16_MAX_WORLD_LINE_TEST_RESULTS = 0;  // absent"),
        constant(module, "KU_FINE_LINE_TEST_WORLD_TYPE_FLAG", "const u32 KU_FINE_LINE_TEST_WORLD_TYPE_FLAG = 0;  // absent"),
    ])
    pieces = {
        "fxsm_ltf_result_ns.inc": result_ns,
        "fxsm_ltf_queue_bodies.inc": bodies,
        "fxsm_ltf_consts.inc": consts,
        "fxsm_ltf_process_fine.inc": fine,
        "fxsm_ltf_process_tri.inc": tri,
        "fxsm_ltf_form.inc": "#define FXSM_LTF_RECORDS_AT_16 " + ("1" if records_at_16 else "0"),
    }
    rc = build_and_run(REPO / "tests" / "FxScenemgrLineTestFine.cpp", pieces, "fxsm_ltf")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
