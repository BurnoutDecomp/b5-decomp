"""Hosted Windows2022/Jobs4-only production Gameplay95 source-slice regression.

All compilation/execution happens inside the existing kill-on-close Windows Job helper.
No execution is performed without --run and the genuine hosted-runner environment.
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
EVIDENCE = TESTS / "evidence/gameplay95"
RESET = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"
CONTACT = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_ValidateRaceCarWorldContact.cpp"
DEBUG = "src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.cpp"
IMMEDIATE = "src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.cpp"
INTERFACE = "src/GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.cpp"
FLAGS = "src/GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h"


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
    good = os.name == "nt" and all(actual[key] == value for key, value in expected.items())
    if not good:
        raise RuntimeError("Gameplay95 requires genuine GitHub-hosted Windows2022 with Jobs4; " + repr(actual))
    return actual


def git_read(arguments):
    executable = shutil.which("git.exe")
    if not executable or not Path(executable).is_absolute():
        raise RuntimeError("absolute git.exe could not be resolved")
    result = subprocess.run([str(Path(executable).resolve()), "-C", str(REPO), *arguments],
                            capture_output=True, check=True)
    return result.stdout


def verify_inputs():
    contract = json.loads((EVIDENCE / "source_contract.json").read_text(encoding="utf-8"))
    plan = json.loads((EVIDENCE / "test_plan.json").read_text(encoding="utf-8"))
    actual_commit = git_read(["rev-parse", "HEAD"]).decode().strip()
    if not re.fullmatch(r"[0-9a-f]{40}", actual_commit):
        raise RuntimeError("candidate HEAD is not a full Git commit")
    if os.environ.get("CANDIDATE_B5_SHA") != actual_commit:
        raise RuntimeError("actual candidate HEAD does not match workflow candidate binding")
    bindings = []
    for source in contract["source_files"]:
        path = REPO / source["path"]
        raw = path.read_bytes()
        expected = source["post_lf_sha256"]
        if digest(canonical(raw)) != expected:
            raise RuntimeError("candidate corrective source differs from reviewed proposal: " + source["path"])
        # Check tracked Git bytes as well as the physical file; no working-tree substitution.
        tracked = git_read(["show", actual_commit + ":" + source["path"]])
        if canonical(raw) != canonical(tracked):
            raise RuntimeError("candidate physical file differs from tracked Git source: " + source["path"])
        bindings.append({"path": source["path"], "source_commit": actual_commit,
                         "raw_sha256": digest(raw), "lf_sha256": expected,
                         "git_blob_sha1": git_read(["rev-parse", actual_commit + ":" + source["path"]]).decode().strip()})
    for provider in contract["providers"]:
        raw = (REPO / provider["path"]).read_bytes()
        if digest(canonical(raw)) != provider["lf_sha256"]:
            raise RuntimeError("protected production/provider input changed: " + provider["path"])
        bindings.append({"path": provider["path"], "raw_sha256": digest(raw),
                         "lf_sha256": provider["lf_sha256"]})
    for fixture in contract["fixture_files"]:
        if digest(canonical((REPO / fixture["path"]).read_bytes())) != fixture["lf_sha256"]:
            raise RuntimeError("fixture/evidence binding changed: " + fixture["path"])
    if len(plan["check_ids"]) != 177 or len(set(plan["check_ids"])) != 177:
        raise RuntimeError("malformed finite check plan")
    if len(plan["original_controls"]["failed_ids"]) != 89:
        raise RuntimeError("malformed original-control check plan")
    return contract, plan, bindings, actual_commit


def contact_block(source):
    begin = source.index("const bool lbIsWall = !lbIsCurb")
    begin = source.index(";", begin) + 1
    end = source.index("        // ---- record the wall triangle on the car's debug component", begin)
    return source[begin:end]


def reset_projection(construct, fields):
    # Preserve exact unconditional assignment statements in original order. An absent
    # original store stays absent, allowing the actual old source to execute and fail.
    names = {field["name"] for field in fields}
    depth = 0
    candidates = []
    ignored = r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    tokens = re.finditer(ignored + r"|[{}]|\b(?:" + "|".join(map(re.escape, sorted(names))) + r")\s*=(?!=)[^;]*;", construct)
    for token in tokens:
        if token[0].startswith(("//", "/*", '"', "'")): continue
        if token[0] == "{": depth += 1
        elif token[0] == "}": depth -= 1
        else:
            name = re.match(r"\w+", token[0])[0]
            if depth != 1:
                raise RuntimeError("selected Construct store is conditional/nested: " + name)
            candidates.append((name, token[0]))
    if len({name for name, _ in candidates}) != len(candidates):
        raise RuntimeError("selected Construct field has multiple stores")
    return "\n".join("    " + statement for _, statement in candidates), [name for name, _ in candidates]


def generate(tree, plan, baseline=None):
    from fxgs_common import definition
    fields = plan["fields"]
    construct = baseline["construct_definition"] if baseline else definition(tree.read(RESET), "void RaceCarEntityModule::Construct()")
    contact = baseline["contact_block"] if baseline else contact_block(tree.read(CONTACT))
    reset, selected = reset_projection(construct, fields)
    if baseline is None and set(selected) != {field["name"] for field in fields}:
        raise RuntimeError("candidate lacks selected actual Construct reset statements")
    flags = tree.read(FLAGS)
    flag_accessors = "\n".join(definition(flags, signature) for signature in
        ("bool RenderWallContacts() const", "bool RenderGroundContacts() const"))
    flag_declarations = []
    for name in ("mbRenderWallContacts", "mbRenderGroundContacts"):
        matches = re.findall(r"\bbool\s+" + name + r"\s*;", flags)
        if len(matches) != 1: raise RuntimeError("ambiguous flag declaration: " + name)
        flag_declarations += matches
    declarations = "struct ResetOwner {\n" + "\n".join("    " + field["type"] + " " + field["name"] + ";" for field in fields)
    declarations += "\n    void Construct();\n};\nstruct ContactFlags {\n" + flag_accessors + "\n" + "\n".join(flag_declarations) + "\n};\n"
    declarations += "struct ContactProducer { ContactFlags mDebugComponent; void Produce(const CgsGeometric::Triangle4::AOSTriangle& lTriangle, bool lbIsWall, bool lbIsCurb); };\n"
    definitions = "void ResetOwner::Construct() {\n" + reset + "\n}\n"
    definitions += "void ContactProducer::Produce(const CgsGeometric::Triangle4::AOSTriangle& lTriangle, bool lbIsWall, bool lbIsCurb) {\n" + contact + "\n}\n"
    debug = tree.read(DEBUG)
    definitions += "namespace CgsDev { namespace {\n"
    helper_signatures = ("rw::RGBA MakeImmediateColour(", "Vector3 MakeVector3(", "Matrix44Affine MakeTransform(", "Matrix44Affine MakeBoxTransform(", "void FillSphereEvent(")
    for signature in helper_signatures:
        if signature == "Matrix44Affine MakeBoxTransform(": definitions += "template <typename BoxEvent>\n"
        if signature == "void FillSphereEvent(": definitions += "template <typename SphereEvent>\n"
        definitions += definition(debug, signature) + "\n"
    definitions += "}\n"
    for signature in ("void DebugRender::Construct()", "void DebugRender::Clear()", "void DebugRender::Destruct()",
                      "void DebugRender::DrawTriangle(", "void DebugRender::DrawSolidSphere(", "void DebugRender::Dispatch3D("):
        definitions += definition(debug, signature) + "\n"
    interface = tree.read(INTERFACE)
    for signature in ("DebugInterface::DebugInterface()", "DebugInterface::~DebugInterface()", "DebugRender& DebugInterface::GetRender()"):
        definitions += definition(interface, signature) + "\n"
    definitions += "}\n"
    immediate = tree.read(IMMEDIATE)
    definitions += "namespace {\n"
    begin = immediate.index("    struct Debug3DRenderStates")
    end = immediate.index("    Vector3 Add3(", begin)
    definitions += immediate[begin:end] + definition(immediate, "Vector3 Add3(") + "\n}\nnamespace CgsDev {\n"
    for signature in ("void Debug3DImmediateRender::Begin(", "void Debug3DImmediateRender::End()",
                      "void Debug3DImmediateRender::SetProjectionMode(", "void Debug3DImmediateRender::SetDrawingMode(",
                      "void Debug3DImmediateRender::DispatchVertices()", "void Debug3DImmediateRender::AddVertex(",
                      "void Debug3DImmediateRender::AddTriangle(", "void Debug3DImmediateRender::AddQuad(",
                      "void Debug3DImmediateRender::DrawSolidTriangle(", "void Debug3DImmediateRender::DrawSolidQuad(",
                      "void Debug3DImmediateRender::DrawTriangle(", "void Debug3DImmediateRender::DrawSolidSphere("):
        definitions += definition(immediate, signature) + "\n"
    definitions += "}\n"
    field_list = "\n".join("FIELD(" + field["type"] + ", " + field["name"] + ")" for field in fields) + "\n"
    return definitions, {"gameplay95_fields.inc": declarations, "gameplay95_field_list.inc": field_list}, {
        "selected_reset_stores": selected, "construct_definition_lf_sha256": digest(construct.encode()),
        "contact_block_lf_sha256": digest(contact.encode()), "generated_definition_lf_sha256": digest(definitions.encode()),
        "baseline": baseline["source_commit"] if baseline else None,
    }


def verify_variant(out, expectation, expected_ids, expected_fail_ids):
    receipt = json.loads((out / "result.json").read_text(encoding="utf-8"))
    stdout = (out / "run.stdout.txt").read_text(encoding="utf-8")
    lines = re.findall(r"^CHECK (PASS|FAIL) ([A-Za-z0-9_.]+)$", stdout, re.M)
    checks = [name for _, name in lines]
    failures = [name for verdict, name in lines if verdict == "FAIL"]
    if re.search(r"^(?:ASSERT|UNEXPECTED)\b", stdout, re.M):
        raise RuntimeError("production assert or unexpected boundary call occurred: " + str(out))
    numeric = receipt.get("numeric")
    expected_numeric = {"checks": expectation["checks"], "failures": expectation["failures"]}
    if numeric != expected_numeric or receipt.get("process_exit_code") != expectation["exit_code"]:
        raise RuntimeError("compiled fixture did not return exact behavioral count/exit: " + str(out))
    if checks != expected_ids or failures != expected_fail_ids:
        raise RuntimeError("raw individual checks differ from exact finite behavioral plan: " + str(out))
    if not receipt.get("executable_sha256"):
        raise RuntimeError("missing compiled executable; compile/link failure is never a negative control")
    if digest((out / "regression.exe").read_bytes()) != receipt["executable_sha256"]:
        raise RuntimeError("executable hash mismatch")
    return {"numeric": numeric, "exit_code": receipt["process_exit_code"], "raw_failed_ids": failures,
            "result_raw_sha256": digest((out / "result.json").read_bytes()),
            "stdout_raw_sha256": digest((out / "run.stdout.txt").read_bytes()),
            "stderr_raw_sha256": digest((out / "run.stderr.txt").read_bytes()),
            "executable_sha256": receipt["executable_sha256"]}


def worker(out):
    from fxgs_common import Tree, compile_and_run, STRSTREAM_CPP
    guard = hosted_guard()
    contract, plan, bindings, commit = verify_inputs()
    tree = Tree(None)
    variants = [("candidate", None)]
    for name in ("6502d53", "11cbd50"):
        baseline = json.loads((EVIDENCE / ("baseline_" + name + ".json")).read_text(encoding="utf-8"))
        variants.append(("original_" + name, baseline))
    records = []
    for name, baseline in variants:
        destination = out / name
        definitions, extra_files, extraction = generate(tree, plan, baseline)
        compile_and_run(TESTS / "Gameplay95.cpp", "gameplay95_production.inc", definitions, "Gameplay95",
                        extra_files=extra_files, extra_sources=(STRSTREAM_CPP, REPO / "src/GameShared/GameClasses/RenderWare/RwRGBA.cpp"),
                        output_dir=destination)
        write_json(destination / "extraction.json", extraction)
        expectation = plan["original_controls"] if baseline else plan["candidate"]
        failed_ids = plan["original_controls"]["failed_ids"] if baseline else []
        records.append({"variant": name, "source_slice_commit": baseline["source_commit"] if baseline else commit,
                        **verify_variant(destination, expectation, plan["check_ids"], failed_ids)})
    result = {"schema": 1, "status": "pass", "candidate_source_commit": commit, "guard": guard,
              "source_contract_raw_sha256": digest((EVIDENCE / "source_contract.json").read_bytes()),
              "bindings": bindings, "variants": records,
              "claim": "177 finite production-source-slice checks; original controls execute and fail exactly 89 functional checks each. Native/rendered-frame behavior remains unverified."}
    write_json(out / "worker_result.json", result)


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
    if workflow != REPO.parent.resolve(): raise RuntimeError("workflow root does not own this candidate checkout")
    out = args.out.resolve()
    artifact_root = workflow / "build/candidate/artifacts"
    if not out.is_relative_to(artifact_root.resolve()) or out == artifact_root.resolve():
        raise RuntimeError("output must be a dedicated retained candidate-artifact directory")
    if args.worker:
        if not out.is_dir() or {path.name for path in out.iterdir()} != {"owned_worker.log"}:
            raise RuntimeError("worker requires the fresh parent-owned transcript directory")
        try: worker(out)
        except Exception as error:
            write_json(out / "worker_failure.json", {"error": str(error), "type": type(error).__name__})
            raise
        return 0
    if out.exists() and any(out.iterdir()): raise RuntimeError("refusing to overwrite retained fixture evidence")
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
              and cleanup.get("stdio_handles_closed") is True and owned.get("transcript_hash_after_cleanup") is True)
    if digest((out / "owned_worker.log").read_bytes()) != owned.get("log_raw_sha256"): passed = False
    worker_file = out / "worker_result.json"
    worker_result = json.loads(worker_file.read_text(encoding="utf-8")) if worker_file.exists() else None
    passed = passed and worker_result is not None and worker_result.get("status") == "pass"
    # After the owned process tree is inactive, independently rejoin every raw receipt.
    if passed:
        _, plan, _, commit = verify_inputs()
        for name in ("candidate", "original_6502d53", "original_11cbd50"):
            is_candidate = name == "candidate"
            verify_variant(out / name, plan["candidate"] if is_candidate else plan["original_controls"],
                           plan["check_ids"], [] if is_candidate else plan["original_controls"]["failed_ids"])
        passed = worker_result["candidate_source_commit"] == commit and len(worker_result["variants"]) == 3
    result = {"schema": 1, "status": "pass" if passed else "fail", "guard": guard, "owned_job": owned,
              "worker_result_raw_sha256": digest(worker_file.read_bytes()) if worker_file.exists() else None,
              "local_author_execution": False}
    write_json(out / "fixture_result.json", result)
    print("Gameplay95 hosted fixture: " + result["status"])
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
