"""Extract and execute actual GUI audio callback/consumer bodies. No game launch.

--expect-original-divergence accepts only the observed ca802 201/112 publication
and original sweep-name failures as a valid negative control; receipts retain the
underlying failing C++ result. Candidate runs require zero numeric failures.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import zlib

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def workflow_root():
    for parent in HERE.parents:
        if (parent / "tools/build/msvc_flags.txt").is_file():
            helper_dir = parent / "b5-decomp/tests"
            required = (helper_dir / "fxgs_common.py", helper_dir / "run_gui_menu_music_wire.py",
                        parent / "tools/build/msvc_includes.txt", parent / "tools/build/msvc_env.bat")
            missing = [str(path) for path in required if not path.is_file()]
            if missing:
                raise RuntimeError("GUI audio regression requires the canonical workflow test/build helpers; "
                                   "missing: " + ", ".join(missing))
            return parent
    raise RuntimeError("GUI audio regression requires a workflow checkout containing "
                       "tools/build/msvc_flags.txt and b5-decomp/tests/fxgs_common.py")


WORKFLOW = workflow_root()
sys.path.insert(0, str(WORKFLOW / "b5-decomp/tests"))
from fxgs_common import definition, compile_and_run
import run_gui_menu_music_wire as existing_wire

MODULE = "src/GameSource/Gui/BrnGuiModule.cpp"
EVENTS = "src/GameSource/Gui/BrnGuiEventTypeDefs.cpp"
SOUND = "src/GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.cpp"
OUTPUT = "src/GameShared/GameClasses/Gui/CgsGuiModuleIO_OutputBuffer.cpp"
PRESENTATION = "src/GameSource/Sound/Global/BrnPresentationEffect.cpp"
ROOTS = existing_wire.HEADER_ROOTS + (
    "src/GameSource/Gui/BrnGuiEventTypeDefs.h",
    "src/GameShared/GameClasses/Gui/CgsGuiModuleIO.h",
    "src/GameShared/GameClasses/Core/CgsStringUtils.h",
    "src/GameShared/GameClasses/System/Resource/CgsResourceID.h",
)


class SourceTree:
    def __init__(self, root):
        self.root = root.resolve()
        self.rev = None
        self.inputs = {}

    def read(self, relative):
        path = self.root / relative
        if not path.is_file():
            return ""
        data = path.read_bytes()
        self.inputs[relative] = hashlib.sha256(data).hexdigest()
        return data.decode("utf-8-sig").replace("\r\n", "\n")

    def required(self, relative):
        value = self.read(relative)
        if not value:
            raise ValueError("Missing production input: " + relative)
        return value


def extract(tree):
    snippets = {}
    def body(key, path, signature):
        snippets[key] = definition(tree.required(path), signature)
        return snippets[key]

    io = tree.required("src/GameShared/GameClasses/Module/CgsIOBuffer.cpp")
    output = "\n".join(body("output_" + name, OUTPUT, signature) for name, signature in (
        ("construct", "void OutputBuffer::Construct()"),
        ("clear", "void OutputBuffer::Clear()"),
        ("get", "const OutputBuffer::GuiEventQueue* OutputBuffer::GetOutEventQueue() const"),
        ("get_nonconst", "OutputBuffer::GuiEventQueue* OutputBuffer::GetOutEventQueueNonConst()"),
    ))
    callback = body("callback", MODULE, "void GuiModule::FlaptSoundTriggerCallback(")
    construct = body("construct", EVENTS, "void GuiAudioTriggerEvent::Construct(")
    names = re.search(r"static const char\* const KAPC_GUI_AUDIO_ACTION_NAMES\[27\]\s*=\s*\{.*?\};",
                      tree.required(EVENTS), re.S)
    if not names:
        raise ValueError("Missing production audio log name table")
    snippets["log_names"] = names[0]
    header = body("wire_header", SOUND, "struct GuiEventWireHeader") + ";"
    get_payload = body("get_payload", SOUND, "const u8* GetGuiPayload(")
    queue = body("queue_sound", SOUND, "template <typename T>\n    void QueueSoundMessage(")
    process = definition(tree.required(SOUND), "void SoundLogicModule::ProcessGuiEvents(")
    cases = definition(process, "case 201:")
    if "case 457:" not in cases:
        raise ValueError("Production extraction did not include case457")
    snippets["case201_457"] = cases
    notify = body("presentation_notify", PRESENTATION, "void PresentationEffect::Notify(")
    entry = body("presentation_entry", "src/GameSource/Sound/Global/BrnPresentationEffect.h",
                 "\n    struct PresentationEntry\n") + ";"
    hash_body = body("hash_string", "src/GameShared/GameClasses/System/Resource/CgsResourceID.cpp",
                     "u64 ID::HashString(")
    sprintf_body = body("sprintf", "src/GameShared/GameClasses/Core/CgsStringUtils.cpp",
                        "void CgsCore::SPrintf(")
    generated = "\n\n".join((io, sprintf_body,
        "namespace CgsGui { namespace CgsGuiModuleIO {", output, "}}",
        "namespace BrnGui {", names[0], construct, callback, "}",
        "namespace CgsResource {", hash_body, "}",
        "namespace BrnSound { namespace Module { namespace {", header, get_payload, queue,
        "} void ProcessAudioCase(CgsModule::VariableEventQueue<8192,16>& mMessageQueue, "
        "s32 liType, const CgsModule::Event* lpEvent, s32 liSize) { "
        "const u8* lpuPayload=GetGuiPayload(lpEvent,liType,liSize); switch(liType) {",
        cases, "}} }}", "namespace BrnSound { namespace Logic {", notify, "}}"))
    return generated, entry, snippets


def oracle():
    table_file = HERE / "data/gui_audio_actions_artist.json"
    table = json.loads(table_file.read_text())["tables"][0]["entries"]
    if len(table) != 14:
        raise ValueError("ARTIST oracle must contain14 actions")
    component, label, movie = "Component_" + "C" * 21, "Label_" + "L" * 25, "Movie_" + "M" * 25
    rows = [(str(row["index"]), row["value"], component, label, movie, row["index"]) for row in table]
    rows += [
        ("fallback", "UNRECOGNIZED_ACTION", component, label, movie, 14),
        ("truncation", "ON_ACCEPT", "Component_" + "C" * 60, "Label_" + "L" * 60, "Movie_" + "M" * 60, 4),
        ("component_fallback", "ON_FOCUS", component, "uninitialised", movie, 2),
    ]
    def crc(value):
        return zlib.crc32(value.lower().encode()) if value else 0
    lines = []
    for name, action, comp, lab, mov, index in rows:
        effective = comp[:31] if lab[:31] == "uninitialised" else lab[:31]
        values = [json.dumps(value) for value in (name, action, comp, lab, mov)]
        lines.append("{" + ",".join(values + [str(index), f"0x{crc(effective):08X}u", f"0x{crc(mov[:31]):08X}u"]) + "},")
    return "\n".join(lines) + "\n", table_file


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--expect-original-divergence", action="store_true")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("--output-dir must be new or empty")
    tree = SourceTree(args.source_root)
    receipt = {"source_root": str(tree.root), "expect_original_divergence": args.expect_original_divergence,
               "scope": "Actual callback, constructor, IO locks/queues, GUI sound case201/457 and PresentationEffect::Notify. Resolve/Play are observed boundaries; no game launch or audible proof."}
    try:
        generated, entry, snippets = extract(tree)
        existing_wire.HEADER_ROOTS = ROOTS
        shadow = existing_wire.source_shadow(tree)
        rows, table_file = oracle()
        result = compile_and_run(HERE / "GuiAudioTrigger.cpp", "gui_audio_extracted.inc", generated,
            "GuiAudioTrigger", shadow=shadow,
            extra_files={"presentation_entry.inc": entry, "audio_oracle.inc": rows,
                         "CgsStrStream.cpp": tree.required("src/GameShared/GameClasses/Development/CgsStrStream.cpp")},
            extra_sources=(output / "CgsStrStream.cpp",), output_dir=output)
        receipt["numeric"] = {"checks": result[0], "failures": result[1]} if result else None
        receipt["inputs"] = tree.inputs
        receipt["extracted_sha256"] = {key: hashlib.sha256(text.encode()).hexdigest() for key, text in snippets.items()}
        receipt["oracle_sha256"] = hashlib.sha256(table_file.read_bytes()).hexdigest()
        stdout = (output / "run.stdout.txt").read_text(errors="replace")
        receipt["observations"] = [line for line in stdout.splitlines() if line.startswith("OBSERVED ")]
        receipt["failures"] = [line for line in stdout.splitlines() if line.startswith("FAIL ")]
        observations = [row for row in re.findall(
            r"OBSERVED callback=(\d+) type=(\d+) size=(\d+) action=(\d+) asserts=(\d+)", stdout)
            if int(row[4]) == 0]
        original = len(observations) == 14 and all(
            int(kind) == 201 and int(size) == 112 and int(asserts) == 0 and
            int(action) == (14 if int(index) in (11,13) else int(index))
            for index, kind, size, action, asserts in observations)
        receipt["original_counterfactual_observed"] = original
        names = [str(index) for index in range(14)] + ["fallback", "truncation", "component_fallback", "0 unlocked"]
        expected = {"FAIL " + name + " " + check for name in names for check in (
            "original GUI id457", "original GUI size100", "raw component offset0", "raw action offset32",
            "raw label offset36", "raw movie offset68")}
        expected.update("FAIL " + str(index) + " " + check for index in (11,13) for check in (
            "sound action", "actual Notify reaches Resolve/Play with original action and name hashes"))
        receipt["original_expected_failure_count"] = len(expected)
        receipt["original_failure_set_matches"] = set(receipt["failures"]) == expected and len(receipt["failures"]) == len(expected)
        receipt["status"] = "compile_or_execution_failure" if result is None else (
            "accepted_original_divergence" if args.expect_original_divergence and original and
                receipt["original_failure_set_matches"] and result[1] == len(expected) else
            "pass" if not args.expect_original_divergence and result[1] == 0 else "fail")
    except Exception as exc:
        receipt["status"] = "extraction_or_setup_failure"
        receipt["error"] = f"{type(exc).__name__}: {exc}"
        print(receipt["error"])
    (output / "fixture.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print("GuiAudioTrigger runner:", receipt["status"])
    return 0 if receipt["status"] in ("pass", "accepted_original_divergence") else 1


if __name__ == "__main__":
    raise SystemExit(main())
