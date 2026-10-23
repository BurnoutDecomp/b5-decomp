"""Hosted-only whole-TU route-save fixture with exact-source negative controls.

No game, disk profile provider, assets or autosave service is started. Root must
integrate reviewed payloads and pin their LF hashes in the hosted candidate gate.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
from online_save_route_binding import (EVIDENCE, compose_variant, identity, read, validate_sources)
from online_save_route_owned import run_owned

SUMMARY = re.compile(r"^ONLINE_SAVE_ROUTE checks=(\d+) failures=(\d+) assertions=(\d+)$", re.MULTILINE)

def write_json(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")

def content_lines(path: Path) -> list[str]:
    return [line.strip() for line in read(path).splitlines() if line.strip() and not line.lstrip().startswith("#")]

def checked_git(args: list[str], root: Path) -> str:
    result = subprocess.run(["git", "-C", str(root), *args], capture_output=True, encoding="utf-8", timeout=20)
    if result.returncode: raise RuntimeError("read-only Git identity command failed")
    return result.stdout.strip()

def assert_checkout(b5: Path, workflow: Path) -> dict:
    b5_sha = os.environ.get("CANDIDATE_B5_SHA", ""); workflow_sha = os.environ.get("CANDIDATE_WORKFLOW_SHA", "")
    if not re.fullmatch(r"[0-9a-f]{40}", b5_sha) or not re.fullmatch(r"[0-9a-f]{40}", workflow_sha):
        raise RuntimeError("exact candidate/workflow SHAs required")
    head = checked_git(["rev-parse", "HEAD"], b5)
    branch = checked_git(["rev-parse", "--abbrev-ref", "HEAD"], b5)
    status = checked_git(["status", "--porcelain=v1", "--untracked-files=all", "--ignore-submodules=none"], b5)
    workflow_head = checked_git(["rev-parse", "HEAD"], workflow)
    if (head, branch, status, workflow_head) != (b5_sha, "HEAD", "", workflow_sha):
        raise RuntimeError("requires clean detached exact hosted candidate and exact workflow checkout")
    return {"b5_sha": head, "b5_branch": branch, "b5_status": status, "workflow_sha": workflow_head,
            "run_id": os.environ.get("GITHUB_RUN_ID"), "run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
            "runner_environment": os.environ.get("CANDIDATE_RUNNER_ENVIRONMENT"),
            "runner_os": os.environ.get("RUNNER_OS"), "runner_arch": os.environ.get("RUNNER_ARCH"),
            "image_os": os.environ.get("ImageOS"), "image_version": os.environ.get("ImageVersion")}

def toolchain() -> tuple[Path, Path, dict]:
    cl = shutil.which("cl.exe"); linker = shutil.which("link.exe")
    if not cl or not linker: raise RuntimeError("canonical x64 MSVC environment must already be configured by workflow")
    cl = Path(cl).resolve(); linker = Path(linker).resolve()
    if cl.parent != linker.parent or os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() != "x64":
        raise RuntimeError("matching x64 cl/link required")
    if os.environ.get("CL") or os.environ.get("_CL_") or os.environ.get("LINK") or os.environ.get("_LINK_"):
        raise RuntimeError("ambient compiler/linker option injection is forbidden")
    tools = [cl, linker] + [cl.parent / name for name in ("c1xx.dll", "c2.dll", "mspdbcore.dll") if (cl.parent / name).is_file()]
    return cl, linker, {"files": {str(path): identity(path) for path in tools},
        "environment": {key: os.environ.get(key) for key in ("VCToolsVersion", "VisualStudioVersion", "WindowsSDKVersion", "VSCMD_ARG_TGT_ARCH", "INCLUDE", "LIB", "VSLANG")}}

def good_run(row: dict, expected_exit: int) -> bool:
    cleanup = row.get("cleanup", {})
    return (row.get("exit_code") == expected_exit and row.get("timed_out") is False and
            row.get("owned_job") is True and row.get("transcript_hash_after_cleanup") is True and
            cleanup.get("complete") is True and cleanup.get("job_active_processes_after_cleanup") == 0)

def dependencies(path: Path, source: Path) -> dict:
    report = json.loads(path.read_text("utf-8-sig")); data = report.get("Data", {})
    if Path(data.get("Source", "")).resolve() != source.resolve(): raise ValueError("MSVC dependency source differs")
    includes = data.get("Includes")
    if not isinstance(includes, list) or not includes: raise ValueError("MSVC dependency include list missing")
    files = [source.resolve(), *[Path(p).resolve() for p in includes]]
    return {str(p): identity(p) for p in dict.fromkeys(files)}

def classify(run: dict, log: Path, expected: dict) -> dict:
    text = log.read_text("utf-8", errors="replace").replace("\r\n", "\n")
    summaries = SUMMARY.findall(text)
    rows = re.findall(r"^CHECK (\S+) (PASS|FAIL)$", text, re.MULTILINE)
    ids = [name for name, status in rows]; failures = [name for name, status in rows if status == "FAIL"]
    valid = (good_run(run, expected["exit_code"]) and len(summaries) == 1 and
        tuple(map(int, summaries[0])) == (len(expected["check_ids"]), len(expected["failure_ids"]), 0) and
        ids == expected["check_ids"] and failures == expected["failure_ids"] and "UNEXPECTED_ASSERT" not in text)
    if expected.get("breakpoint_mode"):
        found = re.findall(r"^ORIGINAL_BREAKPOINT mode=(save|load) code=([0-9A-F]{8})$", text, re.MULTILINE)
        valid = valid and found == [(expected["breakpoint_mode"], "80000003")]
    return {"status": "pass" if valid and not failures else "expected_failure_observed" if valid else "fail",
            "valid": valid, "expected": expected, "observed_check_ids": ids, "observed_failure_ids": failures,
            "summaries": summaries, "run": run}

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    # This guard precedes output creation, Git subprocesses and all native work.
    if (not args.run or os.name != "nt" or os.environ.get("GITHUB_ACTIONS") != "true" or
        os.environ.get("RUNNER_OS") != "Windows" or os.environ.get("RUNNER_ARCH") != "X64" or
        os.environ.get("CANDIDATE_RUNNER_ENVIRONMENT") != "github-hosted"):
        parser.error("execution is available only in the reviewed GitHub-hosted Windows x64 candidate job with --run")
    b5 = Path(__file__).resolve().parents[1]; workflow = args.workflow_root.resolve(); out = args.out.resolve()
    artifact_parent = (workflow / "build/candidate/artifacts").resolve()
    if b5 != (workflow / "b5-decomp").resolve() or not out.is_relative_to(artifact_parent) or out == artifact_parent:
        parser.error("exact b5 checkout and a dedicated candidate artifact subdirectory required")
    if out.exists() and any(out.iterdir()): parser.error("fresh/empty fixture artifact directory required")
    out.mkdir(parents=True, exist_ok=True)
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    os.environ["VSLANG"] = "1033"
    result = {"schema_version": 1, "fixture": "ONLINE_SAVE_ROUTE", "success": False, "executed": False,
              "b5_root": str(b5), "workflow_root": str(workflow), "artifact_root": str(out), "variants": {}}
    try:
        result["checkout"] = assert_checkout(b5, workflow)
        result["sources"] = validate_sources(b5, workflow)
        plan_path = b5 / EVIDENCE / "test_plan.json"; plan = json.loads(plan_path.read_text("utf-8"))
        if set(plan.get("modes", {})) != {"candidate", "omitted_init", "oldtrap_save", "oldtrap_load"}:
            raise ValueError("exact four-mode plan required")
        result["test_plan"] = {"path": str(plan_path), **identity(plan_path)}
        cl, linker, result["toolchain"] = toolchain()
        flags = [part for line in content_lines(workflow / "tools/build/msvc_flags.txt") for part in line.split()]
        if "/Gy" not in flags or "/O2" not in flags: raise ValueError("canonical function-discard flags missing")
        includes = ["/I" + str((workflow / relative).resolve()) for relative in content_lines(workflow / "tools/build/msvc_includes.txt")]
        result["executed"] = True
        for variant in ("candidate", "oldtrap", "omitted_init"):
            dest = out / variant; dest.mkdir()
            composition = compose_variant(b5, dest, variant); sources = composition.pop("sources")
            row = {**composition, "compile": [], "modes": {}, "dependency_inputs": {}}
            result["variants"][variant] = row
            macro = {"candidate": [], "oldtrap": ["/DROUTE_SAVE_OLDTRAP_CONTROL=1"], "omitted_init": ["/DROUTE_SAVE_OMIT_INIT_CONTROL=1"]}[variant]
            objects = []
            for index, source in enumerate(sources):
                obj = dest / f"source{index}.obj"; dep = dest / f"source{index}.dependencies.json"
                command = [str(cl), *flags, "/D_ALLOW_KEYWORD_MACROS=1", *macro, *includes,
                           "/c", str(source), "/Fo" + str(obj), "/sourceDependencies", str(dep)]
                log = dest / f"compile{index}.log"; run = run_owned(command, dest, log, 120)
                row["compile"].append({"command": command, "source": {"path": str(source), **identity(source)}, "run": run})
                if not good_run(run, 0) or not obj.is_file() or not dep.is_file(): raise RuntimeError("fixture compile failed: " + variant)
                row["dependency_inputs"].update(dependencies(dep, source))
                row["compile"][-1]["dependency_report"] = {"path": str(dep), **identity(dep)}
                row["compile"][-1]["object"] = {"path": str(obj), **identity(obj)}
                objects.append(obj)
            exe = dest / "OnlineSaveRoute.exe"
            command = [str(linker), "/nologo", "/MACHINE:X64", "/SUBSYSTEM:CONSOLE", "/OPT:REF", "/INCREMENTAL:NO",
                       "/OUT:" + str(exe), *map(str, objects), "kernel32.lib"]
            row["link"] = {"command": command, "run": run_owned(command, dest, dest / "link.log", 120)}
            if not good_run(row["link"]["run"], 0) or not exe.is_file(): raise RuntimeError("fixture link failed: " + variant)
            row["executable"] = {"path": str(exe), **identity(exe)}
            modes = ("oldtrap_save", "oldtrap_load") if variant == "oldtrap" else (variant,)
            for mode in modes:
                arguments = [mode.removeprefix("oldtrap_")] if variant == "oldtrap" else []
                log = dest / (mode + ".log")
                run = run_owned([str(exe), *arguments], dest, log, 20)
                row["modes"][mode] = classify(run, log, plan["modes"][mode])
                if not row["modes"][mode]["valid"]: raise RuntimeError("fixture behavior witness failed: " + mode)
            for path, before in row["dependency_inputs"].items():
                if identity(Path(path)) != before: raise RuntimeError("compile dependency changed during fixture: " + path)
            row["dependencies_unchanged_after_run"] = True
        if validate_sources(b5, workflow) != result["sources"]: raise RuntimeError("bound inputs changed during run")
        if assert_checkout(b5, workflow) != result["checkout"]: raise RuntimeError("checkout attribution changed during run")
        result["success"] = True; result["status"] = "pass"
        print("ONLINE_SAVE_ROUTE_HOSTED PASS: candidate and three exact negative-control witnesses")
        return 0
    except Exception as error:
        result["status"] = "fail"; result["error"] = str(error)
        print("ONLINE_SAVE_ROUTE_HOSTED FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        write_json(out / "result.json", result)

if __name__ == "__main__":
    raise SystemExit(main())
