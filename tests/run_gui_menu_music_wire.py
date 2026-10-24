"""Compile/run the extracted menu-GUI event 23 consumer and a B595 negative control.

This runner is intended for hosted Windows/MSVC CI. It is deliberately not run from
the proposal workspace. It extracts the case arm and helpers from the selected source
tree, compiles them against that tree's actual headers, and retains all generated inputs,
commands, stdout/stderr, hashes, and numeric receipts under --output-dir.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import posixpath
import re
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def resolve_layout() -> tuple[Path, Path]:
    """Resolve installed tests/ layout or this packet's explicitly validated scratch layout."""
    if HERE.name == "tests":
        b5_root = HERE.parent.resolve()
        workflow = b5_root.parent
    elif HERE.name == "menu23_wire_fixture_20261023_delta04_root":
        workflow = HERE.parents[2].resolve()
        b5_root = workflow / "b5-decomp"
    else:
        raise RuntimeError(
            "Unsupported fixture location; install into b5-decomp/tests or use the named scratch packet")

    if b5_root.name.lower() != "b5-decomp":
        raise RuntimeError("Resolved b5 root has unexpected directory name: " + str(b5_root))
    if not (b5_root / "tests" / "fxgs_common.py").is_file():
        raise RuntimeError("Missing repository fixture helper under " + str(b5_root / "tests"))
    if not (workflow / "tools" / "build" / "msvc_flags.txt").is_file():
        raise RuntimeError("Missing workflow compile settings under " + str(workflow / "tools" / "build"))
    return workflow, b5_root


WORKFLOW, B5_ROOT = resolve_layout()
sys.path.insert(0, str(B5_ROOT / "tests"))

from fxgs_common import Tree, definition, compile_and_run  # noqa: E402

SOUND_CPP = "src/GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.cpp"
BOOT_CPP = "src/GameSource/Gui/Flow/HUD/States/BrnBootLegal.cpp"
STATE_HEADER = "src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
STRSTREAM_CPP = "src/GameShared/GameClasses/Development/CgsStrStream.cpp"
CONTROL_REV = "b595d61d2440ca94d61aa8fa610ab0bf6048cc66"

HEADER_ROOTS = (
    "src/types.hpp",
    "src/GameShared/GameClasses/Core/CgsAssert.h",
    "src/GameShared/GameClasses/Development/CgsStrStream.h",
    "src/GameShared/GameClasses/Development/Log/CgsLog.h",
    "src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h",
    "src/GameShared/GameClasses/Module/CgsVariableEventQueue.h",
    "src/GameShared/GameClasses/Sound/IO/CgsMessage.h",
    "src/GameSource/Sound/Module/LogicModule/BrnMessageData.h",
)


class SourceRootTree:
    """Source reader for an unpacked b5-decomp checkout or candidate source root."""

    def __init__(self, root: Path):
        self.root = root.resolve()
        self.rev = None

    def read(self, relative: str) -> str:
        path = self.root / Path(relative)
        if not path.is_file():
            return ""
        return path.read_text(encoding="utf-8-sig")


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def read_required(tree, relative: str) -> str:
    text = tree.read(relative)
    if not text:
        raise ValueError("missing or empty source input: " + relative)
    return text


def source_shadow(tree) -> dict[str, str]:
    """Shadow the actual queue/message/event headers and their src-local include closure."""
    pending = list(HEADER_ROOTS)
    visited: dict[str, str] = {}
    include_re = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
    while pending:
        relative = posixpath.normpath(pending.pop())
        if relative in visited:
            continue
        text = tree.read(relative)
        if not text:
            if relative in HEADER_ROOTS:
                raise ValueError("missing required header: " + relative)
            continue
        visited[relative] = text
        for include in include_re.findall(text):
            candidates = (
                posixpath.normpath(posixpath.join(posixpath.dirname(relative), include)),
                posixpath.normpath(posixpath.join("src", include)),
            )
            for candidate in candidates:
                if candidate == "src" or candidate.startswith("src/"):
                    if candidate not in visited and tree.read(candidate):
                        pending.append(candidate)
                        break
    return visited


def extract_branch(tree) -> tuple[str, dict[str, str]]:
    """Return generated C++ using exact production declarations, helpers and case 23 arm."""
    sound = read_required(tree, SOUND_CPP)
    boot = read_required(tree, BOOT_CPP)
    state_header = read_required(tree, STATE_HEADER)
    # Select the actual retained record declaration in this source revision.
    # The newer original payload has reclaimed the old name and is only 8 bytes.
    carrier_names = ("GuiMenuStreamRecordPC", "GuiEventPlayMusicOnMenuStream")
    carriers = [name for name in carrier_names
                if len(re.findall(r"(?m)^\s*struct " + re.escape(name) +
                                  r"\s*:\s*public GuiEvent<23>(?=\s*\{)", state_header)) == 1]
    if len(carriers) != 1:
        raise ValueError("requires one actual 20-byte PC GUI-23 record declaration")
    pc_carrier_alias = "using PcMenuStreamRecord = CgsGui::" + carriers[0] + ";"
    process = definition(sound, "void SoundLogicModule::ProcessGuiEvents(")
    case23 = definition(process, "case 23:")
    snippets = {
        "pc_carrier_alias": pc_carrier_alias,
        "wire_header": definition(sound, "struct GuiEventWireHeader") + ";",
        "get_gui_payload": definition(sound, "const u8* GetGuiPayload("),
        "event_s32_at": definition(sound, "s32 EventS32At("),
        "event_u8_at": definition(sound, "u8 EventU8At("),
        "queue_sound_message": definition(sound,
            "template <typename T>\n    void QueueSoundMessage("),
        "post_sound_message": definition(sound,
            "template <typename T>\n    void PostSoundMessage("),
        "case23": case23,
        "channel155_record": definition(boot, "struct MusicOnMenuStreamEvent") + ";",
    }
    generated = [
        "// Generated from the selected production revision by run_gui_menu_music_wire.py.",
        "namespace Menu23Fixture {", snippets["pc_carrier_alias"], snippets["channel155_record"], "}",
        "namespace BrnSound { namespace Module { namespace {",
        snippets["wire_header"], snippets["get_gui_payload"],
        snippets["event_s32_at"], snippets["event_u8_at"],
        snippets["queue_sound_message"], snippets["post_sound_message"],
        "} // anonymous production-helper scope",
        "void ProcessMenu23Case(CgsModule::VariableEventQueue<8192, 16>& mMessageQueue,",
        "                       s32 liType, const CgsModule::Event* lpEvent, s32 liSize)",
        "{",
        "    const u8* lpuPayload = GetGuiPayload(lpEvent, liType, liSize);",
        "    switch (liType) {", snippets["case23"], "    }",
        "}", "}} // namespace BrnSound::Module",
    ]
    source_hashes = {SOUND_CPP: sha256_text(sound), BOOT_CPP: sha256_text(boot),
                     STATE_HEADER: sha256_text(state_header)}
    source_hashes.update({"extracted." + name: sha256_text(text)
                          for name, text in snippets.items()})
    return "\n\n".join(generated) + "\n", source_hashes


def numeric_from_result(output_dir: Path) -> dict | None:
    result_path = output_dir / "result.json"
    if not result_path.is_file():
        return None
    try:
        return json.loads(result_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def run_one(tree, output_dir: Path, tag: str, control: bool) -> tuple[tuple[int, int] | None, dict]:
    generated, source_hashes = extract_branch(tree)
    shadow = source_shadow(tree)
    strstream = read_required(tree, STRSTREAM_CPP)
    extra_flags = "/DMENU23_EXPECT_OLD_RAW_MISMATCH=1" if control else ""
    result = compile_and_run(
        HERE / "GuiMenuMusicWire.cpp",
        "menu23_extracted.inc",
        generated,
        tag,
        extra_flags=extra_flags,
        shadow=shadow,
        extra_sources=(output_dir / "CgsStrStream.cpp",),
        extra_files={"CgsStrStream.cpp": strstream},
        output_dir=output_dir,
    )
    receipt = numeric_from_result(output_dir) or {}
    receipt["source_text_sha256"] = source_hashes
    receipt["shadow_header_count"] = len(shadow)
    receipt["shadow_header_sha256"] = {
        relative: sha256_text(text) for relative, text in sorted(shadow.items())
    }
    receipt["source_tree"] = getattr(tree, "rev", None) or str(tree.root)
    receipt["tag"] = tag
    receipt["compile_and_numeric_output_reached"] = result is not None
    stdout_path = output_dir / "run.stdout.txt"
    stdout_text = stdout_path.read_text(encoding="utf-8", errors="replace") if stdout_path.exists() else ""
    receipt["field_failure_witnesses"] = [line for line in stdout_text.splitlines()
                                          if line.startswith("FAIL ")]
    receipt["field_observations"] = [line for line in stdout_text.splitlines()
                                     if line.startswith("OBSERVED ")]
    receipt["expected_raw8_mismatch_witnesses"] = [
        line for line in stdout_text.splitlines()
        if line.startswith("NEGATIVE_CONTROL_RAW8_MISMATCH ")
    ]
    (output_dir / "result.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return result, receipt


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--rev", help="candidate B5 git revision to read with git show")
    source.add_argument("--source-root", type=Path,
                        help="unpacked b5-decomp root containing src/")
    parser.add_argument("--control-rev", default=CONTROL_REV,
                        help="unchanged old-consumer negative-control revision (default: B595)")
    parser.add_argument("--output-dir", required=True, type=Path,
                        help="new or empty directory for retained compile and numeric receipts")
    args = parser.parse_args()

    output_root = args.output_dir.resolve()
    if output_root.exists() and any(output_root.iterdir()):
        print("Refusing to overwrite non-empty output directory: " + str(output_root), file=sys.stderr)
        return 2
    output_root.mkdir(parents=True, exist_ok=True)

    if args.source_root:
        candidate_tree = SourceRootTree(args.source_root)
        candidate_label = str(candidate_tree.root)
    else:
        candidate_tree = Tree(args.rev)
        candidate_label = args.rev
    control_tree = Tree(args.control_rev)

    manifest = {
        "fixture": "menu GUI event 23 -> sound message 13",
        "status": "starting",
        "candidate_source": candidate_label,
        "negative_control_revision": args.control_rev,
        "negative_control_source": "unchanged B595 SoundLogicModule::ProcessGuiEvents case 23",
        "checks": {
            "candidate": "raw 8-byte body, full 20-byte type-23 record, and translated full type-155 record; each for flags 00/01/10/11",
            "negative_control": "B595 raw-8 case must mismatch numeric fields; its two full-record compatibility shapes must pass",
        },
        "scope_limit": "starts at the queue body delivered to SoundLogicModule::ProcessGuiEvents; does not execute the channel-40 producer drain or prove audible playback",
    }

    try:
        candidate_result, candidate_receipt = run_one(
            candidate_tree, output_root / "candidate", "Menu23Candidate", False)
        control_result, control_receipt = run_one(
            control_tree, output_root / "b595_negative_control", "Menu23OldControl", True)
    except Exception as exc:  # extraction/wiring failure is distinct from numeric failure
        manifest["status"] = "extraction_or_wiring_failure"
        manifest["error"] = f"{type(exc).__name__}: {exc}"
        (output_root / "fixture.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print(manifest["error"], file=sys.stderr)
        return 2

    manifest["runs"] = {
        "candidate": {"numeric": candidate_result, "receipt": "candidate/result.json"},
        "negative_control": {"numeric": control_result,
                             "receipt": "b595_negative_control/result.json"},
    }
    if candidate_result is None or control_result is None:
        manifest["status"] = "compile_or_execution_failure"
        manifest["candidate_receipt"] = candidate_receipt
        manifest["negative_control_receipt"] = control_receipt
        (output_root / "fixture.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        return 2

    candidate_pass = candidate_result[1] == 0
    control_pass = control_result[1] == 0
    manifest["candidate_receipt"] = candidate_receipt
    manifest["negative_control_receipt"] = control_receipt
    numeric_consistent = all(
        receipt.get("numeric") == {"checks": result[0], "failures": result[1]}
        for result, receipt in ((candidate_result, candidate_receipt),
                                (control_result, control_receipt))
    )
    execution_pass = all(
        receipt.get("process_exit_code") == 0
        for receipt in (candidate_receipt, control_receipt)
    )
    if not numeric_consistent:
        manifest["status"] = "compile_or_execution_failure"
    elif not (candidate_pass and control_pass):
        manifest["status"] = "numeric_failure"
    elif not execution_pass:
        manifest["status"] = "compile_or_execution_failure"
    else:
        manifest["status"] = "pass"
    (output_root / "fixture.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Menu23 fixture status: " + manifest["status"])
    return 0 if manifest["status"] == "pass" else (2 if manifest["status"] == "compile_or_execution_failure" else 1)


if __name__ == "__main__":
    raise SystemExit(main())
