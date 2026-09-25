"""FX-FOLLOWUPS (crash parity 2026-09-25, REVIEW-K on b5 35a5e66c): the deepest-volume query event
CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest names its members.

  CgsSceneManagerIO_EventVolumeTestDeepest.h  the DWARF members of DecFIGS CgsSceneManagerIO_FineQuery.h:101-109
                                              (mTransform, mQueryId, mx32EntityTypeFlags, mExcludeEntityId,
                                              meExclusionMode, mVolumeBuffer, mxVolumeTypeFlags) sit BESIDE the
                                              opaque 224-byte payload in an anonymous union, pinned by offsetof to
                                              the producer's stores @0x822170B0
  SceneManagerModule::ProcessVolumeTestDeepest @0x828D4460 reads the event by those names (it read raw payload
                                              offsets before)
  SceneQueryInterface::VolumeTestDeepest @0x822170B0, the producer, is Niaz's and is NOT changed (conductor, option
                                              B); this test proves its bytes land in the named members

Numeric: tests/FxFollowupsVolumeTestDeepestEvent.cpp compiles the revision's event header (shadowed in) and the
revision's producer against a recording queue, and checks the offsetof pins, the byte image the producer queues
against the console's stores, and every named member against its argument. A revision whose header has no named
members cannot build the numeric half: every numeric check then counts as failed.

--hunk <file> also applies the `<<<<<<< OLD SceneQueryInterface::VolumeTestDeepest ... >>>>>>> NEW` block(s) of that
file (a paste-ready by-name rewrite of the producer's stores; scratch/CRASHPARITY_0922/fixes/FX-NET.wip/HANDOFF.md) to
the producer IN MEMORY -- each OLD must match exactly once -- and checks that the rewritten producer writes the same
bytes as the revision's.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_volume_test_deepest_event.py [--rev <b5 rev>] [--hunk <file>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

SM = "src/GameShared/GameClasses/SceneManager/"
EVENT_H = SM + "CgsSceneManagerIO_EventVolumeTestDeepest.h"
SQI_CPP = SM + "CgsSceneManagerIO_SceneQueryInterface.cpp"
MODULE = SM + "CgsSceneManagerModule.cpp"
PRODUCER = "int CgsSceneManager::SceneManagerIO::SceneQueryInterface::VolumeTestDeepest("
CONSUMER = "void SceneManagerModule::ProcessVolumeTestDeepest("
HUNK_LABEL = "SceneQueryInterface::VolumeTestDeepest"
NUMERIC_CHECKS = 17
NUMERIC_CHECKS_HUNK = 19
MEMBERS = ["mTransform", "mQueryId", "mx32EntityTypeFlags", "mExcludeEntityId", "meExclusionMode", "mVolumeBuffer",
           "mxVolumeTypeFlags"]
OFFSETS = {"mTransform": "0x00", "mQueryId": "0x40", "mx32EntityTypeFlags": "0x44", "mExcludeEntityId": "0x48",
           "meExclusionMode": "0x4C", "mVolumeBuffer": "0x50", "mxVolumeTypeFlags": "0xD0"}


def wiring(tree):
    header = code_only(tree.read(EVENT_H))
    element = re.search(r"struct\s+alignas\(16\)\s+InEventVolumeTestDeepest\s*:[^{]*\{(.*?)\n    \};", header, re.S)
    members = element.group(1) if element else ""
    order = r"\s+".join(r"[\w:]+\s+%s\s*(?:\[[^\]]*\])*\s*;" % name for name in MEMBERS)
    union = re.search(r"union\s*\{\s*u8\s+macOpaquePayload\s*\[\s*224\s*\]\s*;\s*struct\s*\{\s*" + order + r"\s*\}\s*;\s*\}\s*;",
                      members)
    pins = all(re.search(r"static_assert\s*\(\s*offsetof\s*\(\s*InEventVolumeTestDeepest\s*,\s*%s\s*\)\s*==\s*%s\b"
                         % (name, OFFSETS[name]), header, re.I) for name in MEMBERS)
    try:
        consumer = code_only(definition(tree.read(MODULE), CONSUMER))
    except ValueError:
        consumer = ""
    by_name = (consumer != ""
               and all(re.search(r"lpQuery\s*->\s*%s\b" % name, consumer) for name in MEMBERS)
               and "macOpaquePayload" not in consumer
               and "reinterpret_cast<const u32*>" not in consumer.replace(" ", "")
               and re.search(r"\blpEvent\b", consumer) is None)
    return [
        ("CgsSceneManagerIO_EventVolumeTestDeepest.h: the union keeps `u8 macOpaquePayload[224];` and names the seven "
         "DWARF members beside it, in the DWARF order (CgsSceneManagerIO_FineQuery.h:103..:109)", union is not None),
        ("CgsSceneManagerIO_EventVolumeTestDeepest.h: an offsetof static_assert pins each member to the producer's "
         "store offset (+0x00 / 0x40 / 0x44 / 0x48 / 0x4C / 0x50 / 0xD0)", pins),
        ("SceneManagerModule::ProcessVolumeTestDeepest reads the event by name: lpQuery->m<member> for all seven, no "
         "payload bytes, no raw-offset u32 reads (REVIEW-K)", by_name),
    ]


def parse_hunks(path):
    """[(label, old, new)] for the OLD/NEW blocks of `path` whose label names the producer."""
    lines = Path(path).read_text(encoding="utf-8").replace("\r\n", "\n").split("\n")
    blocks, state, label, old, new = [], None, None, [], []
    for line in lines:
        if line.startswith("<<<<<<< OLD"):
            state, label, old, new = "old", line[len("<<<<<<< OLD"):].strip(), [], []
        elif line == "=======" and state == "old":
            state = "new"
        elif line.startswith(">>>>>>> NEW") and state == "new":
            if label.startswith(HUNK_LABEL):
                blocks.append((label, "\n".join(old), "\n".join(new)))
            state = None
        elif state == "old":
            old.append(line)
        elif state == "new":
            new.append(line)
    return blocks


def numeric(tree, hunk):
    source = tree.read(SQI_CPP)
    try:
        producer = definition(source, PRODUCER)
    except ValueError as error:
        print("NUMERIC: cannot build -- the producer is absent: " + str(error))
        return None
    by_name = ""
    if hunk:
        blocks = parse_hunks(hunk)
        if not blocks:
            print("NUMERIC: cannot build -- no `<<<<<<< OLD %s` block in %s" % (HUNK_LABEL, hunk))
            return None
        applied = source.replace("\r\n", "\n")
        for label, old, new in blocks:
            count = applied.count(old)
            if count != 1:
                print("NUMERIC: cannot build -- hunk '%s': its OLD matches %d times (must be exactly once)" % (label, count))
                return None
            applied = applied.replace(old, new)
        by_name, renamed = re.subn(r"SceneManagerIO::SceneQueryInterface::VolumeTestDeepest\(",
                                   "SceneManagerIO::ByNameSceneQueryInterface::VolumeTestDeepest(",
                                   definition(applied, PRODUCER))
        if renamed != 1:
            print("NUMERIC: cannot build -- could not rename the hunk-applied producer")
            return None
    config = "#define FXFU_VTDE_BY_NAME %d\n" % (1 if hunk else 0)
    return compile_and_run(Path(__file__).with_name("FxFollowupsVolumeTestDeepestEvent.cpp"), "fxfu_vtde_producer.inc",
                           producer, "FxFollowupsVolumeTestDeepestEvent", shadow={EVENT_H: tree.read(EVENT_H)},
                           extra_files={"fxfu_vtde_config.inc": config, "fxfu_vtde_by_name.inc": by_name})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    parser.add_argument("--hunk", help="a file with a paste-ready by-name rewrite of the producer's stores")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxfollowups_volume_test_deepest_event", wiring(tree), numeric(tree, args.hunk),
                  NUMERIC_CHECKS_HUNK if args.hunk else NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
