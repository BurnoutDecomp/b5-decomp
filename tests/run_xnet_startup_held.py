"""Unevaluated hosted-only regression proposal for the held XNet startup composition."""
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
EVIDENCE = TESTS / "evidence/xnet_startup_held"
PROTOCOL = "src/GameShared/GameClasses/System/GameTalk/X360/CgsGameTalkProtocolX360.cpp"
HEADER = "src/GameShared/GameClasses/System/GameTalk/X360/CgsGameTalkProtocolX360.h"
PROVIDER = "src/GameShared/GameClasses/System/PC/CgsXboxLivePC.cpp"
OLD = "994c1be9976445d86e80308ff4fb72bec4e218db"


def sha(raw): return hashlib.sha256(raw).hexdigest()
def canonical(raw): return raw.decode("utf-8-sig").replace("\r\n", "\n").encode()
def write_json(path, value): path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def guard():
    expected = {"GITHUB_ACTIONS": "true", "RUNNER_OS": "Windows", "RUNNER_ARCH": "X64", "ImageOS": "win22",
                "CANDIDATE_RUNNER_ENVIRONMENT": "github-hosted", "BRN_EXE_JOBS": "4"}
    actual = {key: os.environ.get(key) for key in expected}
    if os.name != "nt" or any(actual[key] != value for key, value in expected.items()):
        raise RuntimeError("Requires genuine hosted Windows2022/X64/Jobs4: " + repr(actual))
    for key in ("GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT"):
        if not re.fullmatch(r"[1-9][0-9]*", os.environ.get(key, "")): raise RuntimeError("Missing hosted run identity")
    return {**actual, "run_id": os.environ["GITHUB_RUN_ID"], "run_attempt": os.environ["GITHUB_RUN_ATTEMPT"], "workflow_sha": os.environ.get("GITHUB_SHA")}


def git_read(args):
    git = shutil.which("git.exe")
    if not git or not Path(git).is_absolute(): raise RuntimeError("Absolute git.exe is required")
    return subprocess.run([str(Path(git).resolve()), "-C", str(REPO), *args], check=True, capture_output=True, timeout=30).stdout


def verify_inputs():
    contract = json.loads((EVIDENCE / "source_contract.json").read_text(encoding="utf-8"))
    plan = json.loads((EVIDENCE / "test_plan.json").read_text(encoding="utf-8"))
    old = json.loads((EVIDENCE / "original_caller.json").read_text(encoding="utf-8"))
    commit = git_read(["rev-parse", "HEAD"]).decode().strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit) or os.environ.get("CANDIDATE_B5_SHA") != commit:
        raise RuntimeError("Candidate HEAD/workflow binding differs")
    bindings = []
    for item in contract["sources"] + contract["providers"] + contract["fixtures"]:
        raw = (REPO / item["path"]).read_bytes()
        tracked = git_read(["show", commit + ":" + item["path"]])
        if sha(canonical(raw)) != item["lf_sha256"] or canonical(raw) != canonical(tracked):
            raise RuntimeError("Source/provider/fixture differs from proposed binding: " + item["path"])
        bindings.append({"path": item["path"], "source_commit": commit, "raw_sha256": sha(raw), "lf_sha256": item["lf_sha256"]})
    for item in contract["workflow_providers"]:
        if sha(canonical((REPO.parent / item["path"]).read_bytes())) != item["lf_sha256"]:
            raise RuntimeError("Canonical compiler provider differs")
    raw = old["content"].encode()
    blob = hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()
    if old["source_commit"] != OLD or sha(raw) != contract["old_caller_raw_sha256"] or blob != contract["old_caller_git_blob_sha1"]:
        raise RuntimeError("Genuine old caller body provenance differs")
    if sum(len(case["check_ids"]) for name, case in plan["cases"].items() if name != "old_caller") != 117:
        raise RuntimeError("Finite candidate plan differs")
    return contract, plan, old, bindings, commit


def generate(old, original):
    from fxgs_common import Tree, definition
    tree = Tree(None)
    provider = tree.read(PROVIDER)
    anchor = "// [PC platform leaf] ARTIST NetConnStartup sets byte 1 only for its -nosecure/-coop\n"
    if provider.count(anchor) != 1: raise RuntimeError("Accepted provider extraction boundary differs")
    provider = provider[provider.index(anchor):]
    protocol = canonical(old["content"].encode()).decode() if original else tree.read(PROTOCOL)
    caller = definition(protocol, "void GameTalkProtocol::Construct(")
    header = tree.read(HEADER)
    alias = re.search(r"typedef std::uintptr_t SOCKET;", header)
    if not alias: raise RuntimeError("Held native socket alias differs")
    context = "namespace CgsGameTalk {\n" + alias[0] + "\nclass GameTalkProtocol { public:\n"
    context += definition(header, "enum EConnectionState") + ";\n"
    for field in ("muBufferMsgStart", "muBufferMsgSize", "mrInactivityTimeout", "meConnectionState", "mListeningSocket", "miListenPort"):
        matches = re.findall(r"(?m)^[ \t]*(?:u32|f32|EConnectionState|SOCKET|s32)[ \t]+" + field + r";", header)
        if len(matches) != 1: raise RuntimeError("Actual scalar field declaration differs: " + field)
        context += matches[0].strip() + "\n"
    context += "static TimerBoundary sInactivityTimer; void Construct(bool, s32); }; }\n"
    context += 'static_assert(std::is_same<CgsGameTalk::SOCKET, ::SOCKET>::value, "held native socket width");\n'
    context += "TimerBoundary CgsGameTalk::GameTalkProtocol::sInactivityTimer;\n"
    for name in ("KU_WINSOCK_VERSION", "KU_XNET_CFG_SIZEOFSTRUCT", "KF_DEFAULT_UPDATE_STEP", "KF_DEFAULT_INACTIVITY_TIMEOUT"):
        match = re.search(r"(?m)^[ \t]*const[ \t]+[^;\n]+\b" + name + r"\s*=\s*[^;]+;", protocol)
        if not match: raise RuntimeError("Actual caller constant differs: " + name)
        context += match[0].strip() + "\n"
    context += "const CgsGameTalk::SOCKET KI_INVALID_SOCKET = INVALID_SOCKET;\n"
    caller = "namespace CgsGameTalk {\n" + caller + "\n}\n"
    return provider, {"xnet_caller_context.inc": context, "xnet_caller_production.inc": caller}, {
        "caller_source_commit": OLD if original else os.environ["CANDIDATE_B5_SHA"], "provider_source_commit": os.environ["CANDIDATE_B5_SHA"],
        "provider_body_lf_sha256": sha(canonical(provider.encode())), "full_construct_lf_sha256": sha(canonical(caller.encode())),
        "context_lf_sha256": sha(canonical(context.encode())), "native_abi_layout_lifetime_boundary": "HOSTED_REGRESSION_PROPOSAL.md"}


def verify_case(out, case, plan):
    row = plan["cases"][case]
    result = json.loads((out / "result.json").read_text(encoding="utf-8"))
    stdout = (out / "run.stdout.txt").read_text(encoding="utf-8")
    observed = re.findall(r"^CHECK (PASS|FAIL) ([A-Za-z0-9_.]+)\r?$", stdout, re.M)
    if [item[1] for item in observed] != row["check_ids"] or [item[1] for item in observed if item[0] == "FAIL"] != row["failed_ids"]:
        raise RuntimeError("Raw individual behavioral checks differ: " + case)
    if "UNEXPECTED" in stdout or result.get("numeric") != {"checks": len(row["check_ids"]), "failures": len(row["failed_ids"])} or result.get("process_exit_code") != row["exit_code"]:
        raise RuntimeError("Case did not execute its exact numeric/exit contract")
    executable = out / "regression.exe"
    if not executable.exists() or sha(executable.read_bytes()) != result.get("executable_sha256"):
        raise RuntimeError("Missing compiled executable; compile failure is not a negative control")
    for item in result["inputs"]:
        if sha(Path(item["path"]).read_bytes()) != item["sha256"]: raise RuntimeError("Retained compiler input changed")
    return {"case": case, "numeric": result["numeric"], "exit_code": result["process_exit_code"],
        "failed_ids": row["failed_ids"], "stdout_raw_sha256": sha((out / "run.stdout.txt").read_bytes()),
        "stderr_raw_sha256": sha((out / "run.stderr.txt").read_bytes()), "result_raw_sha256": sha((out / "result.json").read_bytes()),
        "executable_sha256": result["executable_sha256"]}


def verify_generated(out, old, original):
    provider, extras, receipt = generate(old, original)
    for name, expected in {"xnet_provider_production.inc": provider, **extras}.items():
        if canonical((out / name).read_bytes()) != canonical(expected.encode()): raise RuntimeError("Generated source differs from tracked production")
    if json.loads((out / "extraction.json").read_text(encoding="utf-8")) != receipt: raise RuntimeError("Extraction receipt differs")


def worker(out):
    from fxgs_common import compile_and_run, STRSTREAM_CPP
    contract, plan, old, bindings, commit = verify_inputs()
    results = []
    for case in plan["cases"]:
        # A new executable process per case supplies the real production static
        # state initialization. No fixture reset writes to the provider globals.
        os.environ["BRN_XNET_FIXTURE_SCENARIO"] = case
        provider, extras, extraction = generate(old, case == "old_caller")
        destination = out / case
        compile_and_run(TESTS / "XNetStartupHeld.cpp", "xnet_provider_production.inc", provider, "XNetStartupHeld",
                        extra_files=extras, extra_sources=(STRSTREAM_CPP,), output_dir=destination)
        write_json(destination / "extraction.json", extraction)
        verify_generated(destination, old, case == "old_caller")
        results.append(verify_case(destination, case, plan))
    write_json(out / "worker_result.json", {"status": "pass", "candidate_source_commit": commit, "bindings": bindings,
        "results": results, "claim": "117 candidate source-body checks and genuine old caller14/2 control; injected WSA boundary only, no native network/activation/teardown claim"})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow-root", type=Path, required=True); parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--run", action="store_true"); parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.run: parser.error("Only an explicitly selected genuine hosted job may execute")
    hosted = guard(); workflow = args.workflow_root.resolve(); out = args.out.resolve()
    artifact_root = (workflow / "build/candidate/artifacts").resolve()
    if workflow != REPO.parent.resolve() or Path(os.environ.get("GITHUB_WORKSPACE", "")).resolve() != workflow:
        raise RuntimeError("Actual workflow root does not own candidate")
    if not out.is_relative_to(artifact_root) or out == artifact_root: raise RuntimeError("Require a dedicated retained artifact directory")
    if args.worker:
        if not out.is_dir() or {path.name for path in out.iterdir()} != {"owned_worker.log"}: raise RuntimeError("Worker output is not fresh and parent-owned")
        try: worker(out)
        except Exception as error:
            write_json(out / "worker_failure.json", {"error": str(error), "type": type(error).__name__}); raise
        return 0
    if out.exists() and any(out.iterdir()): raise RuntimeError("Never overwrite retained evidence")
    out.mkdir(parents=True, exist_ok=True)
    from online_save_route_owned import run_owned
    command = [str(Path(sys.executable).resolve()), "-B", str(Path(__file__).resolve()), "--workflow-root", str(workflow), "--out", str(out), "--run", "--worker"]
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    owned = run_owned(command, cwd=workflow, log=out / "owned_worker.log", timeout_seconds=900)
    write_json(out / "owned_job_receipt.json", owned)
    cleanup = owned.get("cleanup", {})
    passed = (owned.get("owned_job") is True and owned.get("exit_code") == 0 and not owned.get("timed_out") and cleanup.get("complete") is True
        and cleanup.get("job_active_processes_after_cleanup") == 0 and cleanup.get("process_handles_closed") is True and cleanup.get("job_handle_closed") is True
        and cleanup.get("stdio_handles_closed") is True and owned.get("transcript_hash_after_cleanup") is True and sha((out / "owned_worker.log").read_bytes()) == owned.get("log_raw_sha256"))
    result_path = out / "worker_result.json"
    saved = json.loads(result_path.read_text(encoding="utf-8")) if result_path.exists() else None
    passed = passed and saved is not None and saved.get("status") == "pass"
    if passed:
        _, plan, old, _, commit = verify_inputs(); rows = []
        for case in plan["cases"]:
            verify_generated(out / case, old, case == "old_caller"); rows.append(verify_case(out / case, case, plan))
        passed = saved["candidate_source_commit"] == commit and saved["results"] == rows
    write_json(out / "fixture_result.json", {"schema": 1, "status": "pass" if passed else "fail", "guard": hosted, "owned_job": owned,
        "worker_result_raw_sha256": sha(result_path.read_bytes()) if result_path.exists() else None, "native_or_activation_credit": 0})
    print("XNet held startup proposal: " + ("pass" if passed else "fail")); return 0 if passed else 1


if __name__ == "__main__": raise SystemExit(main())
