"""Hosted Windows x64 check of whole production allocation/free TUs.

The candidate must contain the exact reviewed three-path proposal. The unchanged
f0b8 control is read from an already-present Git object; this runner never fetches
or changes Git. Both variants link the same harness and their own actual pair.
All child processes use the existing hash-bound Windows Job helper.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys

sys.dont_write_bytecode = True

BASE = "f0b8f37b39b89390f61f1f0264c8b704950fb030"
LOW_CPP = "src/GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.cpp"
LOW_H = "src/GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h"
XBOX_CPP = "src/GameShared/GameClasses/System/PC/CgsXboxPlatformPC.cpp"
HARNESS = "tests/LowMemoryXPhysicalWindows.cpp"
HELPER = "tests/online_save_route_owned.py"
# SHA-256 is over exact Git/LF bytes; checkout CRLF conversion is the only
# normalization. No function extraction, source rewriting, or test-only macro.
CANDIDATE_PINS = {
    LOW_H: (2896, "04a9a8748a1309bf1ad76a87349f98cd9fe3587eddb18eaef778dccf33101ee7"),
    LOW_CPP: (12784, "81d8875ed1cb9c2ddb18d2bd8b19e3643cd4a35cebb2c401391f5055d461b0db"),
    XBOX_CPP: (1360, "b15f665bf0b7a761b3052eb31d66c167f6f8ab84b63b86d591aca47dc0c75dd6"),
    "src/types.hpp": (273, "89b8022b07bab80fb0522f9a57e412761b0b787c6887f45b2653aa897b3aa308"),
    HARNESS: (8876, "c2dc33846a88e054d7fb832b1761e69bd41e4a809ab8c0d0811dae5b2be04d6f"),
    HELPER: (12783, "c21a018a37c094eec2c6befcd19b8862f8c33c84b66f84c33c55eac600649179"),
}
BASE_PINS = {
    LOW_H: (2417, "e6073c0bbc0a97622dc72821c638d32856d85b2f11e9301233ffd88e355d6e10", "812553703eb3316f3cdef17693a5f147f43d3fa8"),
    LOW_CPP: (6008, "70d61436dd21dc294e691e552c4ae11c5a7c313e3bbbbbaa52d231623be2c79d", "354623ea14a999a4142d09acb32918abc1b99903"),
    XBOX_CPP: (1943, "408880df81d5b9dbc2bf9a4e67ea53c07d32cd1f33456d4c66e0511836ee9ee4", "6aafc70c86057340a7658572a4fdc5061fa6ebae"),
    "src/types.hpp": (273, "89b8022b07bab80fb0522f9a57e412761b0b787c6887f45b2653aa897b3aa308", None),
}
WORKFLOW_PINS = {
    "tools/build/msvc_flags.txt": (2431, "f409e36fd3bb17cd8ced47f33fb9488c667a260d4dd274a7b6a1a6643356bfc2"),
    "tools/build/msvc_includes.txt": (1453, "e08ea1f7a1243ec960cc41d4d2137796e86a13e949cd6e59fe5baf6c82e80acc"),
    "tools/build/msvc_env.bat": (3978, "64a57b7edcbc7ba793316cdafc19a74b64106ae276f3771d93996e766c76f3b6"),
}
REQUESTS = (
    ("default16", 4096, 0, 4),
    ("xma_input", 1179648, 2048, 0x404),
    ("xma_output", 1572864, 2048, 4),
    ("gui_movie", 16777216, 128, 4),
)
SUMMARY = re.compile(
    r"ALLOCATION_FAMILY_RESULT cases=4 passes=(\d+) family_failures=(\d+) "
    r"other_failures=(\d+) page_bytes=(\d+) peak_requested_bytes=16777216 ulong_bytes=4 pointer_bytes=8")


def identity(path: Path) -> dict:
    raw = path.read_bytes()
    lf = raw.replace(b"\r\n", b"\n")
    return {"raw_bytes": len(raw), "raw_sha256": hashlib.sha256(raw).hexdigest(),
            "lf_bytes": len(lf), "lf_sha256": hashlib.sha256(lf).hexdigest()}


def write_json(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def source_pins(b5: Path, workflow: Path) -> dict:
    files = {}
    for label, root, pins in (("b5", b5, CANDIDATE_PINS), ("workflow", workflow, WORKFLOW_PINS)):
        for relative, (size, digest) in pins.items():
            path = (root / relative).resolve()
            if not path.is_relative_to(root) or not path.is_file():
                raise RuntimeError("missing bound source: " + relative)
            got = identity(path)
            if (got["lf_bytes"], got["lf_sha256"]) != (size, digest):
                raise RuntimeError("bound source differs: " + relative)
            files[label + "/" + relative] = {"path": str(path), **got}
    files["runner"] = {"path": str(Path(__file__).resolve()), **identity(Path(__file__).resolve())}
    return files


def good_run(row: dict, expected_exit: int) -> bool:
    cleanup = row.get("cleanup", {})
    return (row.get("exit_code") == expected_exit and row.get("timed_out") is False and
            row.get("owned_job") is True and row.get("transcript_hash_after_cleanup") is True and
            cleanup.get("complete") is True and cleanup.get("job_active_processes_after_cleanup") == 0)


class Owned:
    def __init__(self, helper: Path, git: Path, out: Path, result: dict):
        spec = importlib.util.spec_from_file_location("low_memory_exact_owned_helper", helper)
        if spec is None or spec.loader is None:
            raise RuntimeError("cannot load the exact owned-process helper")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.run_owned = module.run_owned
        self.git = git
        self.out = out
        self.result = result

    def run(self, command: list[str], cwd: Path, log: Path, seconds: int) -> dict:
        run = self.run_owned(command, cwd, log, seconds)
        # The helper hashes only after its Job and inherited child handles close.
        if identity(log)["raw_sha256"] != run.get("log_raw_sha256"):
            raise RuntimeError("stable owned transcript hash differs")
        return run

    def git_read(self, root: Path, arguments: list[str], label: str) -> bytes:
        if not re.fullmatch(r"[a-z0-9_]+", label):
            raise RuntimeError("invalid Git evidence label")
        log = self.out / ("git_" + label + ".log")
        command = [str(self.git), "--no-optional-locks", "-C", str(root), *arguments]
        run = self.run(command, root, log, 20)
        self.result["git_reads"].append({"command": command, "run": run})
        if not good_run(run, 0):
            raise RuntimeError("read-only Git check failed: " + label +
                               "; Root must prepare/fetch the exact baseline before this runner")
        return log.read_bytes()


def checkout(owned: Owned, b5: Path, workflow: Path, phase: str) -> dict:
    expected_b5 = os.environ.get("CANDIDATE_B5_SHA", "")
    expected_workflow = os.environ.get("CANDIDATE_WORKFLOW_SHA", "")
    if not all(re.fullmatch(r"[0-9a-f]{40}", sha) for sha in (expected_b5, expected_workflow)):
        raise RuntimeError("exact candidate and workflow SHA environment bindings required")
    def read(root: Path, args: list[str], name: str) -> str:
        return owned.git_read(root, args, phase + "_" + name).decode("utf-8", errors="strict").strip()
    head = read(b5, ["rev-parse", "HEAD"], "head")
    branch = read(b5, ["rev-parse", "--abbrev-ref", "HEAD"], "branch")
    status = read(b5, ["status", "--porcelain=v1", "--untracked-files=all", "--ignore-submodules=none"], "status")
    workflow_head = read(workflow, ["rev-parse", "HEAD"], "workflow")
    baseline = read(b5, ["rev-parse", "--verify", BASE + "^{commit}"], "baseline_commit")
    if (head, branch, status, workflow_head, baseline) != (expected_b5, "HEAD", "", expected_workflow, BASE):
        raise RuntimeError("requires clean detached exact candidate, exact workflow and exact existing baseline commit")
    return {"b5_sha": head, "b5_branch": branch, "b5_status": status,
            "workflow_sha": workflow_head, "baseline_sha": baseline}


def toolchain() -> tuple[Path, Path, Path, dict]:
    paths = [shutil.which(name) for name in ("cl.exe", "link.exe", "git.exe")]
    if any(path is None for path in paths):
        raise RuntimeError("workflow must configure canonical MSVC x64 and Git first")
    cl, linker, git = (Path(path).resolve() for path in paths)
    if cl.parent != linker.parent or os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() != "x64":
        raise RuntimeError("matching native x64 compiler and linker required")
    if any(os.environ.get(name) for name in ("CL", "_CL_", "LINK", "_LINK_")):
        raise RuntimeError("ambient compiler/linker options forbidden")
    tools = [cl, linker, git, Path(sys.executable).resolve()]
    tools += [cl.parent / name for name in ("c1xx.dll", "c2.dll", "mspdbcore.dll") if (cl.parent / name).is_file()]
    return cl, linker, git, {"files": {str(path): identity(path) for path in tools},
        "python_version": sys.version,
        "environment": {name: os.environ.get(name) for name in (
            "VCToolsVersion", "VCToolsInstallDir", "VisualStudioVersion", "WindowsSDKVersion",
            "VSCMD_ARG_TGT_ARCH", "INCLUDE", "LIB", "VSLANG")}}


def baseline_sources(owned: Owned, b5: Path, dest: Path) -> dict:
    files = {}
    for index, (relative, (size, digest, blob)) in enumerate(BASE_PINS.items()):
        # cat-file stdout is preserved as bytes. No decode, newline conversion,
        # preprocessor substitution or graft is applied to either production TU.
        raw = owned.git_read(b5, ["cat-file", "blob", BASE + ":" + relative], "baseline_blob_" + str(index))
        got_blob = hashlib.sha1(b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw).hexdigest()
        if len(raw) != size or hashlib.sha256(raw).hexdigest() != digest or (blob is not None and got_blob != blob):
            raise RuntimeError("exact unchanged f0b8 baseline blob differs: " + relative)
        path = dest / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
        files[relative] = {"path": str(path), "git_blob_sha1": got_blob, **identity(path)}
    return files


def dependencies(path: Path, source: Path) -> dict:
    data = json.loads(path.read_text("utf-8-sig")).get("Data", {})
    if Path(data.get("Source", "")).resolve() != source.resolve():
        raise RuntimeError("MSVC dependency source differs")
    includes = data.get("Includes")
    if not isinstance(includes, list) or not includes:
        raise RuntimeError("MSVC dependency include list missing")
    paths = [source.resolve(), *(Path(name).resolve() for name in includes)]
    return {str(path): identity(path) for path in dict.fromkeys(paths)}


def classify(run: dict, log: Path, variant: str) -> dict:
    lines = log.read_text("utf-8", errors="strict").splitlines()
    rows = [json.loads(line.removeprefix("ALLOCATION_CASE ")) for line in lines[:-1]
            if line.startswith("ALLOCATION_CASE ")]
    summary = SUMMARY.fullmatch(lines[-1]) if lines else None
    expected_candidate = variant == "candidate"
    errors = []
    if not good_run(run, 0 if expected_candidate else 31):
        errors.append("wrong exit, timeout or owned-process cleanup")
    if len(lines) != 5 or len(rows) != 4 or summary is None:
        errors.append("exact four JSON witnesses and one terminal summary required")
    page = int(summary[4]) if summary else 0
    if page <= 0 or page & (page - 1):
        errors.append("invalid Windows page size")
    if summary and tuple(map(int, summary.group(1, 2, 3))) != ((4, 0, 0) if expected_candidate else (0, 4, 0)):
        errors.append("summary differs from exact candidate/control outcome")
    common = {"queried": 1, "aligned": 1, "writable": 1, "touched": 1,
              "release_called": 1, "release_queried": 1, "region_state": 0x1000,
              "region_protect": 4, "region_allocation_protect": 4, "region_type": 0x20000}
    for row, (name, size, alignment, protection) in zip(rows, REQUESTS):
        try:
            for field in ("pointer", "allocation_base", "region_base"):
                if not isinstance(row[field], str) or not re.fullmatch(r"0x[0-9a-f]{1,16}", row[field]):
                    raise ValueError("invalid address witness")
            pointer, base, region = (int(row[field], 16) for field in ("pointer", "allocation_base", "region_base"))
            effective = alignment or 16
            expected = {**common, "name": name, "bytes": size, "requested_alignment": alignment,
                        "effective_alignment": effective, "xbox_protection": protection,
                        "allocation_base_equal": int(expected_candidate),
                        "whole_reservation_low": int(expected_candidate),
                        "reservation_released": int(expected_candidate),
                        "reservation_bytes": ((size + page - 1) // page * page) if page and expected_candidate else 0,
                        "touched_pages": ((size + page - 1) // page) if page else 0,
                        "outcome": "PASS" if expected_candidate else "ALLOCATION_BASE_MISMATCH",
                        "requested_span_low": int(0 < pointer < 0x100000000 and size <= 0x100000000 - pointer)}
            if any(row.get(key) != value or type(row.get(key)) is not type(value) for key, value in expected.items()):
                raise ValueError("case contract differs")
            if not pointer or not base or pointer % effective or (pointer == base) != expected_candidate:
                raise ValueError("actual allocation-base/alignment witness differs")
            if region > pointer or not isinstance(row["region_bytes"], int) or row["region_bytes"] <= pointer - region:
                raise ValueError("pointer is outside queried Windows region")
            if expected_candidate and (not row["requested_span_low"] or region != pointer or row["release_state"] != 0x10000):
                raise ValueError("strict low reservation or full release differs")
        except (KeyError, ValueError, TypeError) as error:
            errors.append(name + ": " + str(error))
    return {"valid": not errors, "expected_variant": variant, "errors": errors,
            "cases": rows, "summary": lines[-1] if lines else None,
            "stdout": {"path": str(log), **identity(log)}, "run": run}


def content_lines(path: Path) -> list[str]:
    return [line.strip() for line in path.read_text("utf-8-sig").splitlines()
            if line.strip() and not line.lstrip().startswith("#")]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    # Guard precedes output creation, helper import, Git, compiler and native work.
    if (not args.run or os.name != "nt" or os.environ.get("GITHUB_ACTIONS") != "true" or
        os.environ.get("RUNNER_OS") != "Windows" or os.environ.get("RUNNER_ARCH") != "X64" or
        os.environ.get("CANDIDATE_RUNNER_ENVIRONMENT") != "github-hosted"):
        parser.error("requires --run in the reviewed GitHub-hosted Windows x64 candidate job")
    b5 = Path(__file__).resolve().parents[1]
    workflow = args.workflow_root.resolve()
    out = args.out.resolve()
    artifact_parent = (workflow / "build/candidate/artifacts").resolve()
    if b5 != (workflow / "b5-decomp").resolve() or not out.is_relative_to(artifact_parent) or out == artifact_parent:
        parser.error("exact candidate checkout and dedicated artifact subdirectory required")
    if out.exists() and (not out.is_dir() or any(out.iterdir())):
        parser.error("fresh empty fixture artifact directory required")
    out.mkdir(parents=True, exist_ok=True)
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    os.environ["VSLANG"] = "1033"
    result = {"schema_version": 1, "fixture": "LOW_MEMORY_XPHYSICAL_WINDOWS",
              "success": False, "native_executed": False, "git_reads": [], "variants": {},
              "b5_root": str(b5), "workflow_root": str(workflow), "artifact_root": str(out),
              "host": {name: os.environ.get(name) for name in ("GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT",
                  "CANDIDATE_RUNNER_ENVIRONMENT", "RUNNER_OS", "RUNNER_ARCH", "ImageOS", "ImageVersion")}}
    try:
        result["sources"] = source_pins(b5, workflow)
        cl, linker, git, result["toolchain"] = toolchain()
        owned = Owned(b5 / HELPER, git, out, result)
        result["checkout"] = checkout(owned, b5, workflow, "before")
        flags = [flag for line in content_lines(workflow / "tools/build/msvc_flags.txt") for flag in line.split()]
        if "/Gy" not in flags or "/O2" not in flags:
            raise RuntimeError("canonical optimization/function-discard compile flags required")
        includes = ["/I" + str((workflow / relative).resolve())
                    for relative in content_lines(workflow / "tools/build/msvc_includes.txt")]
        for variant in ("baseline", "candidate"):
            dest = out / variant
            dest.mkdir()
            row = {"compile": [], "dependency_inputs": {}}
            result["variants"][variant] = row
            root = b5
            variant_includes = includes
            if variant == "baseline":
                root = dest / "unchanged_f0b8"
                row["baseline_sources"] = baseline_sources(owned, b5, root)
                variant_includes = ["/I" + str(root / "src"), *includes]
            sources = [b5 / HARNESS, root / LOW_CPP, root / XBOX_CPP]
            objects = []
            for index, source in enumerate(sources):
                obj = dest / ("source" + str(index) + ".obj")
                dep = dest / ("source" + str(index) + ".dependencies.json")
                command = [str(cl), *flags, *variant_includes, "/c", str(source),
                           "/Fo" + str(obj), "/sourceDependencies", str(dep)]
                receipt = {"command": command, "source": {"path": str(source), **identity(source)}}
                row["compile"].append(receipt)
                receipt["run"] = owned.run(command, dest, dest / ("compile" + str(index) + ".log"), 120)
                if not good_run(receipt["run"], 0) or not obj.is_file() or not dep.is_file():
                    raise RuntimeError("actual production TU compile failed: " + variant)
                receipt["object"] = {"path": str(obj), **identity(obj)}
                receipt["dependency_report"] = {"path": str(dep), **identity(dep)}
                compiled_inputs = dependencies(dep, source)
                if source != b5 / HARNESS:
                    required = {str((root / LOW_H).resolve()), str((root / "src/types.hpp").resolve())}
                    # Old XboxPlatformPC has no game-header include; its original
                    # low-memory TU must still resolve the exact baseline header.
                    if source == root / LOW_CPP and not required.issubset(compiled_inputs):
                        raise RuntimeError("whole production low-memory TU used the wrong header/type provider")
                for path, info in compiled_inputs.items():
                    if path in row["dependency_inputs"] and row["dependency_inputs"][path] != info:
                        raise RuntimeError("compile dependency changed between production TUs")
                    row["dependency_inputs"][path] = info
                objects.append(obj)
            exe = dest / "LowMemoryXPhysicalWindows.exe"
            link_map = dest / "LowMemoryXPhysicalWindows.map"
            command = [str(linker), "/nologo", "/MACHINE:X64", "/SUBSYSTEM:CONSOLE", "/OPT:REF",
                       "/INCREMENTAL:NO", "/VERBOSE:LIB", "/MAP:" + str(link_map),
                       "/OUT:" + str(exe), *map(str, objects), "kernel32.lib"]
            row["link"] = {"command": command, "run": owned.run(command, dest, dest / "link.log", 120)}
            if not good_run(row["link"]["run"], 0) or not exe.is_file() or not link_map.is_file():
                raise RuntimeError("actual production TU link failed: " + variant)
            row["executable"] = {"path": str(exe), **identity(exe)}
            row["link_map"] = {"path": str(link_map), **identity(link_map)}
            result["native_executed"] = True
            log = dest / "stdout.log"
            run = owned.run([str(exe)], dest, log, 20)
            row["execute"] = {"command": [str(exe)], "run": run,
                              "stdout": {"path": str(log), **identity(log)}}
            try:
                witness = classify(run, log, variant)
            except (UnicodeError, json.JSONDecodeError) as error:
                witness = {"valid": False, "expected_variant": variant,
                           "errors": ["malformed stdout witness: " + str(error)],
                           "stdout": row["execute"]["stdout"], "run": run}
            witness_path = dest / "witness.json"
            write_json(witness_path, witness)
            row["behavior"] = {"valid": witness["valid"], "errors": witness["errors"], "run": run,
                               "witness": {"path": str(witness_path), **identity(witness_path)}}
            if not witness["valid"]:
                raise RuntimeError("exact allocation-family witness failed: " + variant)
            if identity(exe) != {key: value for key, value in row["executable"].items() if key != "path"}:
                raise RuntimeError("executable changed during run")
            for path, before in row["dependency_inputs"].items():
                if identity(Path(path)) != before:
                    raise RuntimeError("compile dependency changed during fixture: " + path)
            row["dependency_inputs_unchanged_after_run"] = True
        if source_pins(b5, workflow) != result["sources"]:
            raise RuntimeError("bound production/fixture/helper/workflow source changed during run")
        for path, before in result["toolchain"]["files"].items():
            if identity(Path(path)) != before:
                raise RuntimeError("compiler/linker/runtime tool changed during run")
        if checkout(owned, b5, workflow, "after") != result["checkout"]:
            raise RuntimeError("exact checkout attribution changed during run")
        result["success"] = True
        result["status"] = "pass"
        print("LOW_MEMORY_XPHYSICAL_WINDOWS_HOSTED PASS: four candidate reservations and four unchanged-f0b8 allocation-base mismatches")
        return 0
    except Exception as error:
        result["status"] = "fail"
        result["error"] = str(error)
        print("LOW_MEMORY_XPHYSICAL_WINDOWS_HOSTED FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        write_json(out / "result.json", result)


if __name__ == "__main__":
    raise SystemExit(main())
