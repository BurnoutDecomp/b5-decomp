"""Hosted Windows2022/Jobs4-only font/buddy production-body regression.

Complete production dispatchers and real queues run inside the existing owned
Windows Job helper. This author packet has never evaluated this runner locally.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent
EVIDENCE = TESTS / "evidence/font_buddy"
VIEW = "src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp"
VIEW_H = "src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
AUX = "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.cpp"
AUX_H = "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"
HANDLER_H = "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"
CALLBACK = "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.cpp"
CALLBACK_H = "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.h"
BUDDY = "src/GameSource/Network/Managers/BrnNetworkBuddyManagerBase.cpp"
BUDDY_H = "src/GameSource/Network/Managers/BrnNetworkBuddyManagerBase.h"
LEAF = "src/GameSource/Network/Managers/X360/BrnNetworkBuddyManagerX360.cpp"
LEAF_H = "src/GameSource/Network/Managers/X360/BrnNetworkBuddyManagerX360.h"
SDK_H = "src/GameShared/GameClasses/Network/Buddies/DirtySock/CgsBuddyManagerDirtySock.h"
SDK_LEAF_H = "src/GameShared/GameClasses/Network/Buddies/DirtySock/X360/CgsBuddyManagerDirtySockX360.h"
PRODUCER = "src/GameSource/Network/Debug Components/BrnNetworkBuddyManagerDebugComponent.cpp"
PRODUCER_H = "src/GameSource/Network/Debug Components/BrnNetworkBuddyManagerDebugComponent.h"
NETWORK_IO = "src/GameSource/Network/BrnNetworkModuleIO.h"
IMAGE = "src/GameSource/Network/Debug Components/BrnNetworkImageManagerDebugComponent.cpp"
OLD_COMMIT = "1887d92de05d915d53bc4f7645acf84daa39d1bc"
VARIANTS = ("candidate", "original_view_1887", "original_buddy_1887")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def canonical(data):
    return data.decode("utf-8-sig").replace("\r\n", "\n").encode("utf-8")


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def hosted_guard():
    expected = {"GITHUB_ACTIONS": "true", "RUNNER_OS": "Windows", "RUNNER_ARCH": "X64", "ImageOS": "win22",
                "CANDIDATE_RUNNER_ENVIRONMENT": "github-hosted", "BRN_EXE_JOBS": "4"}
    actual = {key: os.environ.get(key) for key in expected}
    if os.name != "nt" or any(actual[key] != value for key, value in expected.items()):
        raise RuntimeError("FontBuddy requires genuine GitHub-hosted Windows2022/Jobs4; " + repr(actual))
    if not re.fullmatch(r"[1-9][0-9]*", os.environ.get("GITHUB_RUN_ID", "")) or not re.fullmatch(r"[1-9][0-9]*", os.environ.get("GITHUB_RUN_ATTEMPT", "")):
        raise RuntimeError("Missing actual hosted run/attempt identity")
    return {**actual, "github_run_id": os.environ["GITHUB_RUN_ID"], "github_run_attempt": os.environ["GITHUB_RUN_ATTEMPT"],
        "github_sha": os.environ.get("GITHUB_SHA"), "github_workflow_ref": os.environ.get("GITHUB_WORKFLOW_REF"),
        "github_repository": os.environ.get("GITHUB_REPOSITORY"), "image_version": os.environ.get("ImageVersion")}


def git_read(arguments):
    executable = shutil.which("git.exe")
    if not executable or not Path(executable).is_absolute():
        raise RuntimeError("absolute git.exe could not be resolved")
    return subprocess.run([str(Path(executable).resolve()), "-C", str(REPO), *arguments], capture_output=True, check=True, timeout=30).stdout


def verify_inputs():
    contract = json.loads((EVIDENCE / "source_contract.json").read_text(encoding="utf-8"))
    plan = json.loads((EVIDENCE / "test_plan.json").read_text(encoding="utf-8"))
    actual_commit = git_read(["rev-parse", "HEAD"]).decode().strip()
    if not re.fullmatch(r"[0-9a-f]{40}", actual_commit) or os.environ.get("CANDIDATE_B5_SHA") != actual_commit:
        raise RuntimeError("actual tracked candidate HEAD does not match workflow binding")
    bindings = []
    for item in contract["source_files"] + contract["providers"] + contract["fixture_files"]:
        path = item["path"]
        raw = (REPO / path).read_bytes()
        tracked = git_read(["show", actual_commit + ":" + path])
        if digest(canonical(raw)) != item["lf_sha256"] or canonical(raw) != canonical(tracked):
            raise RuntimeError("pinned physical/tracked source/provider/fixture changed: " + path)
        bindings.append({"path": path, "source_commit": actual_commit, "raw_sha256": digest(raw),
            "lf_sha256": digest(canonical(raw)), "git_blob_sha1": git_read(["rev-parse", actual_commit + ":" + path]).decode().strip()})
    for item in contract["workflow_providers"]:
        raw = (REPO.parent / item["path"]).read_bytes()
        if digest(canonical(raw)) != item["lf_sha256"]:
            raise RuntimeError("canonical compiler provider changed: " + item["path"])
        bindings.append({"path": "workflow/" + item["path"], "raw_sha256": digest(raw), "lf_sha256": item["lf_sha256"]})
    originals = json.loads((EVIDENCE / "original_control_sources.json").read_text(encoding="utf-8"))
    if originals["source_commit"] != OLD_COMMIT or set(originals["files"]) != {VIEW, BUDDY}:
        raise RuntimeError("original controls do not name the exact finite old source pair")
    for path, item in originals["files"].items():
        raw = item["content"].encode("utf-8")
        blob = hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()
        expected = contract["original_source_files"][path]
        if digest(raw) != expected["raw_sha256"] or blob != expected["git_blob_sha1"]:
            raise RuntimeError("retained unchanged source control drift: " + path)
    if len(plan["check_ids"]) != 232 or len(set(plan["check_ids"])) != 232:
        raise RuntimeError("finite232-check plan changed")
    if len(plan["variants"]["original_view_1887"]["failed_ids"]) != 55 or len(plan["variants"]["original_buddy_1887"]["failed_ids"]) != 16:
        raise RuntimeError("genuine functional negative-control plan changed")
    return contract, plan, originals, bindings, actual_commit


def generate(tree, plan, originals, variant):
    from fxgs_common import definition
    extracted = []

    def full(path, signature, override=None):
        text = override if override is not None else tree.read(path)
        value = definition(text, signature)
        extracted.append({"path": path, "signature": signature, "source_commit": OLD_COMMIT if override is not None else os.environ["CANDIDATE_B5_SHA"],
            "complete_definition_lf_sha256": digest(canonical(value.encode()))})
        return value

    def inline(path, name):
        text = tree.read(path)
        match = re.search(r"(?m)^[ \t]*(?:const[ \t]+void\*|s32|f32|BrnNetworkModule\*)[ \t]+" + re.escape(name) + r"\(", text)
        if not match:
            raise RuntimeError("missing production inline getter: " + name)
        start = match.start()
        end = text.index("{", start)
        return full(path, text[start:end])

    def decl(path, name):
        text = re.sub(r"//[^\n]*|/\*[\s\S]*?\*/", "", tree.read(path))
        pattern = (r"(?m)^[ \t]*(?:(?:static|virtual)[ \t]+)?[\w:*<>]+(?:[ \t]+[\w:*<>]+)*[ \t]+"
                   + re.escape(name) + r"\([^;{}]*\)[ \t]*(?:const[ \t]*)?(?:override[ \t]*)?(?:=[ \t]*0[ \t]*)?;")
        matches = re.findall(pattern, text)
        if len(matches) != 1:
            raise RuntimeError("ambiguous/missing actual declaration " + path + ":" + name)
        return matches[0].strip()

    def field(path, name):
        text = re.sub(r"//[^\n]*|/\*[\s\S]*?\*/", "", tree.read(path))
        matches = re.findall(r"(?m)^[ \t]*(?:const[ \t]+)?[\w:*<>]+(?:[ \t]+[\w:*<>]+)*[ \t]+" + re.escape(name) + r"(?:\[[^\]]+\])?;", text)
        if len(matches) != 1:
            raise RuntimeError("ambiguous/missing actual field " + path + ":" + name)
        return matches[0].strip()

    old_view = canonical(originals["files"][VIEW]["content"].encode()).decode() if variant == "original_view_1887" else None
    old_buddy = canonical(originals["files"][BUDDY]["content"].encode()).decode() if variant == "original_buddy_1887" else None
    getter_text = "\n".join(inline(HANDLER_H, name) for name in ("GetFontCollection", "GetTextEffect", "GetFontSizeScale"))
    handler_fields = "\n".join(field(HANDLER_H, name) for name in ("mpFontCollection", "miTextEffect", "mfFontSizeScale"))
    handler_decls = "\n".join(decl(HANDLER_H, name) for name in ("GetUnusedAptString", "AptStringToZID"))
    aux_decls = "\n".join(decl(AUX_H, name) for name in ("DebugRotateToNextFontEffect", "DebugChangeFontSpacing"))
    callback_decls = "\n".join(decl(CALLBACK_H, name) for name in ("AcquireStringSlot", "DeallocateString"))
    view_decls = "\n".join(decl(VIEW_H, name) for name in ("ProcessIncomingViewEvents", "ProcessIncomingLanguageEvent", "ProcessIncomingLoadNotification",
        "ProcessIncomingUnloadRequestNotification", "ProcessIncomingAptEvent", "SetClearScreenAlpha"))
    # Declarations are an explicit access/storage projection; no original object is
    # cast to this context and no native member offset or vtable slot is asserted.
    context = "namespace CgsGui { class CgsAptString; namespace ViewIO { struct OutputBuffer; }\n"
    context += "class AptRenderHandler { public:\n" + getter_text + "\n" + handler_decls + "\n" + handler_fields + "\n};\n"
    context += "class AptAux { public:\n" + aux_decls + "\n" + field(AUX_H, "mRenderHandler") + "\n};\n"
    context += "class AptAuxPointer { public: static AptAux* mpAptAuxInst; };\nclass AptCallbackRender { public:\n" + callback_decls + "\n};\n"
    context += "struct CustomRendererManager { virtual void RecvEvent(const CgsModule::Event*, s32) = 0; };\n"
    context += "class ViewModule { public:\n" + view_decls + "\nAptAux mAptAux; CustomRendererManager* mpCustomRendererManager; bool mbClearScreenEnabled; f32 mfHack_LastValidTimeStep;\n}; }\n"
    network_io = tree.read(NETWORK_IO)
    constant = re.search(r"const s32 KI_NETWORK_EVENT_QUEUE_SIZE\s*=\s*14000;", network_io)
    if not constant:
        raise RuntimeError("actual network queue capacity changed")
    context += "namespace BrnNetwork { using CgsNetwork::PlayerName; namespace BrnNetworkModuleIO {\n" + constant[0] + "\n"
    context += definition(network_io, "struct NetworkEventQueue :") + "; } }\n"
    context += "namespace CgsNetwork { class BuddyManagerBase { public:\n"
    context += "\n".join(decl(SDK_H, name) for name in ("JoinBuddy", "SendInvite", "CancelInvites", "AcceptInvite", "RevokeInvite", "DeclineInvite", "GetNumBuddies"))
    context += "\n}; class BuddyManagerX360 : public BuddyManagerBase { public:\n" + decl(SDK_LEAF_H, "ShowProfile") + "\n}; }\n"
    context += "namespace BrnNetwork { class BuddyManagerBase; class BuddyManagerDebugComponent { public:\n"
    context += definition(tree.read(PRODUCER_H), "struct BuddyInformation") + ";\n"
    context += "static const s32 KI_MAX_BUDDIES = 5;\n"
    context += "\n".join(field(PRODUCER_H, name) for name in ("mBuddyInformation", "miBuddyIndex", "miMessageIndex", "miFeedbackIndex", "mpEventQueue")) + "\n"
    context += "\n".join(decl(PRODUCER_H, name) for name in ("PrintMessage", "SendTestMessage", "PrintNextUnreadMessage", "LeaveFeedback", "JoinBuddy", "ProcessOutgoingEvents")) + "\n"
    context += definition(tree.read(PRODUCER_H), "BrnNetworkModuleIO::NetworkEventQueue* GetEventQueue()") + "\n};\n"
    context += "class BrnNetworkModule { public: BrnNetworkModuleIO::NetworkEventQueue* queue; BrnNetworkModuleIO::NetworkEventQueue* GetNetworkEventQueue() { return queue; } };\n"
    context += "class BuddyManagerBase : public CgsNetwork::BuddyManagerX360 { public:\n"
    context += "\n".join(decl(BUDDY_H, name) for name in ("ProcessEvent", "ProcessDebugEvents", "ProcessNetworkQueue", "SendEmptyBuddyInformation")) + "\n"
    context += inline(BUDDY_H, "GetNetworkModule") + "\nBuddyManagerDebugComponent mDebugComponent; BrnNetworkModule* mpNetworkModule; s32 miNextBuddyToSend;\n};\n"
    context += "class BuddyManagerX360 : public BuddyManagerBase { public:\n" + decl(LEAF_H, "ProcessNetworkQueue") + "\n};\n"
    context += "class ImageManagerDebugComponent { public: static void EncodeMugshotAtSelectedQuality(void*); }; }\n"

    definitions = "namespace {\n" + full(VIEW, "bool HudVisWitnessEnabled()") + "\n}\nnamespace CgsGui { namespace {\n"
    definitions += definition(tree.read(VIEW), "struct ClearScreenBody") + "; }\n"
    definitions += full(VIEW, "void ViewModule::ProcessIncomingViewEvents(", old_view) + "\n"
    for signature in ("void AptAux::DebugRotateToNextFontEffect()", "void AptAux::DebugChangeFontSpacing("):
        definitions += full(AUX, signature) + "\n"
    definitions += full(CALLBACK, "void* AptCallbackRender::AcquireStringSlot(") + "\n}\nnamespace BrnNetwork { namespace {\n"
    definitions += definition(tree.read(BUDDY), "struct BuddyCountEvent") + ";\n"
    definitions += "typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueueConcrete;\n"
    definitions += full(LEAF, "inline NetworkEventQueueConcrete* AsNetworkQueue(") + "\n"
    definitions += definition(tree.read(LEAF), "struct BuddyListUpdatedEvent") + ";\n"
    for name in ("KI_BUDDY_LIST_UPDATED_EVENT_TYPE", "KI_NETWORK_EVENT_BUDDY_ONLINE", "KI_NETWORK_EVENT_BUDDY_OFFLINE", "KI_NETWORK_EVENT_BUDDY_JOIN", "KI_NETWORK_EVENT_SHOW_PROFILE"):
        match = re.search(r"const s32 " + name + r"\s*=\s*[^;]+;", tree.read(LEAF))
        if not match:
            raise RuntimeError("missing actual platform event constant: " + name)
        definitions += match[0] + "\n"
    producer = tree.read(PRODUCER)
    begin = producer.index("const s32 KI_EVENT_GET_BUDDY_COUNT")
    end = producer.index(";", producer.index("const char* const KPC_TEST_MESSAGE", begin)) + 1
    definitions += producer[begin:end] + "\n}\n"
    definitions += full(BUDDY, "static BrnNetworkModuleIO::NetworkEventQueue* GetEventQueue(") + "\n"
    definitions += full(BUDDY, "void BuddyManagerBase::ProcessEvent(", old_buddy) + "\n"
    definitions += full(BUDDY, "void BuddyManagerBase::ProcessDebugEvents()") + "\n"
    definitions += full(LEAF, "void BuddyManagerX360::ProcessNetworkQueue(") + "\n"
    for name in ("PrintMessage", "SendTestMessage", "PrintNextUnreadMessage", "LeaveFeedback", "JoinBuddy"):
        definitions += full(PRODUCER, "void BuddyManagerDebugComponent::" + name + "(") + "\n"
    definitions += full(IMAGE, "void ImageManagerDebugComponent::EncodeMugshotAtSelectedQuality(") + "\n}\n"
    cases = "struct EffectCase { const char* id; u32 before, after; };\nstatic const EffectCase kEffectCases[] = {\n"
    cases += "\n".join('{"' + item["id"] + '", ' + item["before"] + 'u, ' + item["after"] + 'u},' for item in plan["effect_cases"]) + "\n};\n"
    cases += "struct SpacingCase { const char* id; u32 bits; };\nstatic const SpacingCase kSpacingCases[] = {\n"
    cases += "\n".join('{"' + item["id"] + '", ' + item["bits"] + 'u},' for item in plan["spacing_cases"]) + "\n};\n"
    if len(extracted) != 20:
        raise RuntimeError("complete function/getter extraction inventory changed")
    return definitions, {"font_buddy_context.inc": context, "font_buddy_cases.inc": cases}, {
        "variant": variant, "complete_definitions": extracted, "context_lf_sha256": digest(canonical(context.encode())),
        "generated_definition_lf_sha256": digest(canonical(definitions.encode())),
        "boundary_document": "tests/evidence/font_buddy/BOUNDARIES.md",
        "control_scope": "Only the named original complete dispatcher is replaced; common new helpers remain candidate production code, so absence of an old helper can never count as a functional control failure."}


def verify_generated(out, plan, originals, name):
    # Hosted, read-only re-derivation joins retained compiler input bytes to
    # tracked production source. This performs no compilation or C++ execution.
    from fxgs_common import Tree
    definitions, extras, extraction = generate(Tree(None), plan, originals, name)
    expected = {"font_buddy_production.inc": definitions, **extras}
    for relative, value in expected.items():
        if canonical((out / relative).read_bytes()) != canonical(value.encode()):
            raise RuntimeError("retained generated compiler input does not match actual source: " + relative)
    if json.loads((out / "extraction.json").read_text(encoding="utf-8")) != extraction:
        raise RuntimeError("retained extraction receipt does not match actual source")


def verify_variant(out, plan, name):
    expectation = plan["variants"][name]
    receipt = json.loads((out / "result.json").read_text(encoding="utf-8"))
    stdout = (out / "run.stdout.txt").read_text(encoding="utf-8")
    stderr = (out / "run.stderr.txt").read_text(encoding="utf-8")
    checks = re.findall(r"^CHECK (PASS|FAIL) ([A-Za-z0-9_.]+)\r?$", stdout, re.M)
    if re.search(r"^(?:ASSERT|UNEXPECTED)\b", stdout, re.M):
        raise RuntimeError("unexpected production assert/boundary call in " + name)
    if [item[1] for item in checks] != plan["check_ids"] or [item[1] for item in checks if item[0] == "FAIL"] != expectation["failed_ids"]:
        raise RuntimeError("individual raw functional check roster differs in " + name)
    if receipt.get("numeric") != {"checks": 232, "failures": len(expectation["failed_ids"])} or receipt.get("process_exit_code") != expectation["exit_code"]:
        raise RuntimeError("compiled fixture did not execute exact counts/exit in " + name)
    executable = out / "regression.exe"
    if not executable.is_file() or digest(executable.read_bytes()) != receipt.get("executable_sha256"):
        raise RuntimeError("missing/mismatched retained executable; compile/link failure is never a control")
    for item in receipt["inputs"]:
        if digest(Path(item["path"]).read_bytes()) != item["sha256"]:
            raise RuntimeError("retained compiler/fixture input changed after execution")
    return {"variant": name, "numeric": receipt["numeric"], "process_exit_code": receipt["process_exit_code"],
        "raw_failed_ids": expectation["failed_ids"], "result_raw_sha256": digest((out / "result.json").read_bytes()),
        "stdout_raw_sha256": digest((out / "run.stdout.txt").read_bytes()), "stderr_raw_sha256": digest((out / "run.stderr.txt").read_bytes()),
        "executable_sha256": receipt["executable_sha256"], "extraction_raw_sha256": digest((out / "extraction.json").read_bytes()),
        "generated_definition_lf_sha256": digest(canonical((out / "font_buddy_production.inc").read_bytes())),
        "generated_context_lf_sha256": digest(canonical((out / "font_buddy_context.inc").read_bytes())),
        "generated_cases_lf_sha256": digest(canonical((out / "font_buddy_cases.inc").read_bytes()))}


def worker(out):
    from fxgs_common import Tree, compile_and_run, STRSTREAM_CPP
    guard = hosted_guard()
    contract, plan, originals, bindings, commit = verify_inputs()
    os.environ["BRN_HUD_VIS"] = "0"  # Isolated fixture's real diagnostic gate only; no game configuration.
    tree = Tree(None)
    records = []
    for name in VARIANTS:
        destination = out / name
        definitions, extra_files, extraction = generate(tree, plan, originals, name)
        compile_and_run(TESTS / "FontBuddy.cpp", "font_buddy_production.inc", definitions, "FontBuddy",
                        extra_files=extra_files, extra_sources=(STRSTREAM_CPP,), output_dir=destination)
        write_json(destination / "extraction.json", extraction)
        verify_generated(destination, plan, originals, name)
        records.append(verify_variant(destination, plan, name))
    write_json(out / "worker_result.json", {"schema": 1, "status": "pass", "candidate_source_commit": commit,
        "guard": guard, "bindings": bindings, "variants": records,
        "source_contract_raw_sha256": digest((EVIDENCE / "source_contract.json").read_bytes()),
        "claim": "232 finite actual-body checks; genuine1887 view and buddy controls execute and fail55/16 behavioral checks. Full input bridge, engine/layout ABI, SDK/overlay and native pixels remain unverified."})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.run:
        parser.error("authoring/inspection mode never executes; hosted execution requires --run")
    guard = hosted_guard()
    workflow = args.workflow_root.resolve()
    if workflow != REPO.parent.resolve() or Path(os.environ.get("GITHUB_WORKSPACE", "")).resolve() != workflow:
        raise RuntimeError("actual hosted workflow root does not own this candidate checkout")
    out = args.out.resolve(); artifact_root = (workflow / "build/candidate/artifacts").resolve()
    if not out.is_relative_to(artifact_root) or out == artifact_root:
        raise RuntimeError("output must be a dedicated retained candidate-artifact directory")
    if args.worker:
        if not out.is_dir() or {path.name for path in out.iterdir()} != {"owned_worker.log"}:
            raise RuntimeError("worker requires the fresh parent-owned transcript directory")
        try:
            worker(out)
        except Exception as error:
            write_json(out / "worker_failure.json", {"error": str(error), "type": type(error).__name__})
            raise
        return 0
    if out.exists() and any(out.iterdir()):
        raise RuntimeError("refusing to overwrite retained fixture evidence")
    out.mkdir(parents=True, exist_ok=True)
    from online_save_route_owned import run_owned
    command = [str(Path(sys.executable).resolve()), "-B", str(Path(__file__).resolve()),
               "--workflow-root", str(workflow), "--out", str(out), "--run", "--worker"]
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    owned = run_owned(command, cwd=workflow, log=out / "owned_worker.log", timeout_seconds=900)
    write_json(out / "owned_job_receipt.json", owned)
    cleanup = owned.get("cleanup", {})
    passed = (owned.get("owned_job") is True and owned.get("exit_code") == 0 and not owned.get("timed_out")
        and cleanup.get("complete") is True and cleanup.get("job_active_processes_after_cleanup") == 0
        and cleanup.get("process_handles_closed") is True and cleanup.get("job_handle_closed") is True
        and cleanup.get("stdio_handles_closed") is True and owned.get("transcript_hash_after_cleanup") is True
        and digest((out / "owned_worker.log").read_bytes()) == owned.get("log_raw_sha256"))
    worker_file = out / "worker_result.json"
    worker_result = json.loads(worker_file.read_text(encoding="utf-8")) if worker_file.exists() else None
    passed = passed and worker_result is not None and worker_result.get("status") == "pass"
    if passed:
        _, plan, originals, _, commit = verify_inputs()
        actual_records = []
        for name in VARIANTS:
            verify_generated(out / name, plan, originals, name)
            actual_records.append(verify_variant(out / name, plan, name))
        passed = worker_result["candidate_source_commit"] == commit and worker_result["variants"] == actual_records
    result = {"schema": 1, "status": "pass" if passed else "fail", "guard": guard, "owned_job": owned,
        "worker_result_raw_sha256": digest(worker_file.read_bytes()) if worker_file.exists() else None,
        "local_author_execution": False, "native_or_final_disposition_credit": 0}
    write_json(out / "fixture_result.json", result)
    print("FontBuddy hosted fixture: " + result["status"])
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
