"""Actual PC profile-settings API and complete SystemUserProfile/listener contract.

Canonical MSVC sources, an exact bef9346f9 API/consumer negative control, LF-content
pins, owned bounded Windows children, and artifact-only writes. No game is started.
"""
from __future__ import annotations
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from fxgs_common import definition

TAG = "SYSTEM_USER_PROFILE"
PIN_PATH = "tests/evidence/system_user_profile_pc/source_pins.json"
PLAN_PATH = "tests/evidence/system_user_profile_pc/test_plan.json"
BASE = "bef9346f9dc61f2ae1770bf3c9dbdc89fe4f15a7"
EXPECTED_CHECKS = 82
BASELINE_IDENTITIES = {'tests/evidence/system_user_profile_pc/CgsGuideIntegration_bef9346f9.cpp': {'original_source_path': 'src/GameShared/GameClasses/Gui/CgsGuideIntegration.cpp', 'original_git_lf_blob_sha1': 'dbf6f7a8a47889c3358f30787676ad994e2f9494'}, 'tests/evidence/system_user_profile_pc/CgsXboxLivePC_bef9346f9.cpp': {'original_source_path': 'src/GameShared/GameClasses/System/PC/CgsXboxLivePC.cpp', 'original_git_lf_blob_sha1': '46218b2dc5ca92b78668faf71b0318996dbaa3e0'}}
SUMMARY = re.compile(r"^SYSTEM_USER_PROFILE checks=(\d+) failures=(\d+) assertions=(\d+)$", re.MULTILINE)
PROVIDER = "src/GameShared/GameClasses/System/PC/CgsXboxLivePC.cpp"
CONSUMER = "src/GameShared/GameClasses/Gui/CgsGuideIntegration.cpp"
LEGACY_PROVIDER = "tests/evidence/system_user_profile_pc/CgsXboxLivePC_bef9346f9.cpp"
LEGACY_CONSUMER = "tests/evidence/system_user_profile_pc/CgsGuideIntegration_bef9346f9.cpp"
SPRINTF_DEFINITION_SHA256 = "ca8ec6863b82da887387d5f211a63e3d36593f3bde74fd0ec3387362e2bf69c7"
PINNED_SOURCES = {
    PROVIDER, CONSUMER,
    "src/GameShared/GameClasses/System/PC/CgsXUserProfileSettings.h",
    "src/GameShared/GameClasses/Gui/CgsGuideIntegration.h",
    "src/GameShared/GameClasses/System/PC/CgsPcNetIdentity.cpp",
    "src/GameShared/GameClasses/System/PC/CgsPcNetIdentity.h",
    "src/GameShared/GameClasses/System/CgsHarnessSlot.h",
    "src/GameShared/GameClasses/System/CgsXOverlapped.h",
    "src/GameShared/GameClasses/System/X360/CgsXOverlappedX360.cpp",
    "src/GameShared/GameClasses/Core/CgsStringUtils.cpp",
    "src/GameShared/GameClasses/Core/CgsStringUtils.h",
    "src/GameShared/GameClasses/Core/CgsAssert.h",
    "src/types.hpp", "config.ini.example",
    "tests/SystemUserProfilePC.cpp", "tests/run_system_user_profile_pc.py",
    "tests/fxgs_common.py", LEGACY_PROVIDER, LEGACY_CONSUMER, PLAN_PATH,
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def identity(path: Path) -> dict:
    raw = path.read_bytes()
    lf = raw.replace(b"\r\n", b"\n")
    pairs = raw.count(b"\r\n")
    bare_lf = raw.count(b"\n") - pairs
    bare_cr = raw.count(b"\r") - pairs
    return {"actual_raw_sha256": hashlib.sha256(raw).hexdigest(), "actual_raw_bytes": len(raw),
            "actual_lf_sha256": hashlib.sha256(lf).hexdigest(), "actual_lf_bytes": len(lf),
            "actual_raw_crlf_pairs": pairs, "actual_raw_bare_lf": bare_lf, "actual_raw_bare_cr": bare_cr,
            "actual_raw_eol_variant": "contains_bare_cr" if bare_cr else
                "mixed_crlf_lf" if pairs and bare_lf else "crlf" if pairs else "lf" if bare_lf else "no_newlines"}


def source_receipt(b5: Path, workflow: Path) -> dict:
    pins_path = b5 / PIN_PATH
    pins = json.loads(pins_path.read_text("utf-8"))
    if pins.get("schema_version") != 2 or pins.get("normalization") != "crlf_to_lf_only" or pins.get("base_head") != BASE:
        raise ValueError("unsupported source pin contract")
    if set(pins.get("files", {})) != PINNED_SOURCES:
        raise ValueError("pins must cover exact candidate, fixture and baseline source composition")
    rows = {}
    for relative, expected in pins["files"].items():
        path = (b5 / relative).resolve()
        if not path.is_relative_to(b5) or path == b5:
            raise ValueError("pin path escapes B5")
        if set(expected) != {"expected_lf_sha256", "expected_lf_bytes"} or not re.fullmatch(r"[0-9a-f]{64}", expected["expected_lf_sha256"]):
            raise ValueError("invalid source pin row")
        actual = identity(path)
        rows[relative] = expected | actual | {"matches_lf_content":
            actual["actual_lf_sha256"] == expected["expected_lf_sha256"] and
            actual["actual_lf_bytes"] == expected["expected_lf_bytes"]}
    if len(pins.get("baseline_evidence", [])) != 2 or {r.get("path") for r in pins["baseline_evidence"]} != set(BASELINE_IDENTITIES):
        raise ValueError("exact two original source controls required")
    baseline = []
    for row in pins["baseline_evidence"]:
        if {k: row.get(k) for k in ("original_source_path", "original_git_lf_blob_sha1")} != BASELINE_IDENTITIES[row["path"]]:
            raise ValueError("original baseline identity changed")
        raw_lf = (b5 / row["path"]).read_bytes().replace(b"\r\n", b"\n")
        actual_blob = hashlib.sha1(b"blob " + str(len(raw_lf)).encode() + b"\0" + raw_lf).hexdigest()
        baseline.append(row | {"actual_checkout_lf_git_blob_sha1": actual_blob,
                               "matches_original_git_lf_blob": actual_blob == row["original_git_lf_blob_sha1"]})
    return {"schema_version": 2, "normalization": "crlf_to_lf_only", "base_head": BASE,
            "pins_file": identity(pins_path), "files": rows, "baseline_evidence": baseline,
            "mismatches": [p for p, r in rows.items() if not r["matches_lf_content"]],
            "canonical_tool_files": {p: identity(workflow / p) for p in
                ("tools/build/msvc_flags.txt", "tools/build/msvc_includes.txt", "tools/build/msvc_env.bat")}}


def content_lines(path: Path) -> list[str]:
    return [line.strip() for line in path.read_text("utf-8-sig").splitlines()
            if line.strip() and not line.lstrip().startswith("#")]


def toolchain_receipt() -> tuple[Path, dict]:
    found = shutil.which("cl")
    if not found:
        raise RuntimeError("cl.exe unavailable; configure canonical x64 MSVC environment first")
    compiler = Path(found).resolve()
    files = [compiler, compiler.parent / "link.exe"]
    if not files[1].is_file():
        raise RuntimeError("matching link.exe unavailable")
    for name in ("c1xx.dll", "c2.dll", "mspdbcore.dll"):
        if (compiler.parent / name).is_file(): files.append(compiler.parent / name)
    env = {name: os.environ.get(name) for name in
           ("VCToolsVersion", "VisualStudioVersion", "VSCMD_ARG_TGT_ARCH", "WindowsSDKVersion", "CL", "_CL_")}
    if env["CL"] or env["_CL_"]:
        raise RuntimeError("CL/_CL_ must be unset for pinned canonical compilation")
    return compiler, {"files": {str(p): sha256(p) for p in files}, "environment": env}


def run_owned(command: list[str], cwd: Path, log: Path, timeout_seconds: int) -> dict:
    """Assign a suspended child to a kill-on-close Windows Job, then resume it.

    No PID searches, shell, taskkill, global process cleanup, or external helper.
    A job-assignment failure kills the still-suspended child and fails closed.
    Compiler/linker descendants inherit the same owned job.
    """
    if os.name != "nt":
        raise RuntimeError("this fixture requires the Windows x64 MSVC host")
    import msvcrt

    class IO_COUNTERS(ctypes.Structure):
        _fields_ = [(name, ctypes.c_ulonglong) for name in
                    ("ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
                     "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

    class BASIC_LIMIT(ctypes.Structure):
        _fields_ = [("PerProcessUserTimeLimit", ctypes.c_longlong),
                    ("PerJobUserTimeLimit", ctypes.c_longlong),
                    ("LimitFlags", wintypes.DWORD),
                    ("MinimumWorkingSetSize", ctypes.c_size_t),
                    ("MaximumWorkingSetSize", ctypes.c_size_t),
                    ("ActiveProcessLimit", wintypes.DWORD),
                    ("Affinity", ctypes.c_size_t),
                    ("PriorityClass", wintypes.DWORD),
                    ("SchedulingClass", wintypes.DWORD)]

    class EXTENDED_LIMIT(ctypes.Structure):
        _fields_ = [("BasicLimitInformation", BASIC_LIMIT), ("IoInfo", IO_COUNTERS),
                    ("ProcessMemoryLimit", ctypes.c_size_t),
                    ("JobMemoryLimit", ctypes.c_size_t),
                    ("PeakProcessMemoryUsed", ctypes.c_size_t),
                    ("PeakJobMemoryUsed", ctypes.c_size_t)]

    class STARTUPINFO(ctypes.Structure):
        _fields_ = [("cb", wintypes.DWORD), ("lpReserved", wintypes.LPWSTR),
                    ("lpDesktop", wintypes.LPWSTR), ("lpTitle", wintypes.LPWSTR),
                    ("dwX", wintypes.DWORD), ("dwY", wintypes.DWORD),
                    ("dwXSize", wintypes.DWORD), ("dwYSize", wintypes.DWORD),
                    ("dwXCountChars", wintypes.DWORD), ("dwYCountChars", wintypes.DWORD),
                    ("dwFillAttribute", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
                    ("wShowWindow", wintypes.WORD), ("cbReserved2", wintypes.WORD),
                    ("lpReserved2", ctypes.POINTER(ctypes.c_byte)),
                    ("hStdInput", wintypes.HANDLE), ("hStdOutput", wintypes.HANDLE),
                    ("hStdError", wintypes.HANDLE)]

    class PROCESSINFO(ctypes.Structure):
        _fields_ = [("hProcess", wintypes.HANDLE), ("hThread", wintypes.HANDLE),
                    ("dwProcessId", wintypes.DWORD), ("dwThreadId", wintypes.DWORD)]

    class BASIC_ACCOUNTING(ctypes.Structure):
        _fields_ = [("TotalUserTime", ctypes.c_longlong), ("TotalKernelTime", ctypes.c_longlong),
                    ("ThisPeriodTotalUserTime", ctypes.c_longlong),
                    ("ThisPeriodTotalKernelTime", ctypes.c_longlong),
                    ("TotalPageFaultCount", wintypes.DWORD), ("TotalProcesses", wintypes.DWORD),
                    ("ActiveProcesses", wintypes.DWORD), ("TotalTerminatedProcesses", wintypes.DWORD)]

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
    kernel.CreateJobObjectW.restype = wintypes.HANDLE
    kernel.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int,
                                               ctypes.c_void_p, wintypes.DWORD]
    kernel.SetInformationJobObject.restype = wintypes.BOOL
    kernel.CreateProcessW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR,
        ctypes.c_void_p, ctypes.c_void_p, wintypes.BOOL, wintypes.DWORD,
        ctypes.c_void_p, wintypes.LPCWSTR, ctypes.POINTER(STARTUPINFO), ctypes.POINTER(PROCESSINFO)]
    kernel.CreateProcessW.restype = wintypes.BOOL
    kernel.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    kernel.AssignProcessToJobObject.restype = wintypes.BOOL
    kernel.ResumeThread.argtypes = [wintypes.HANDLE]
    kernel.ResumeThread.restype = wintypes.DWORD
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.WaitForSingleObject.restype = wintypes.DWORD
    kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel.GetExitCodeProcess.restype = wintypes.BOOL
    kernel.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel.TerminateProcess.restype = wintypes.BOOL
    kernel.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel.TerminateJobObject.restype = wintypes.BOOL
    kernel.QueryInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int,
        ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    kernel.QueryInformationJobObject.restype = wintypes.BOOL
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL

    job = kernel.CreateJobObjectW(None, None)
    if not job:
        raise ctypes.WinError(ctypes.get_last_error())
    info = PROCESSINFO()
    started = False
    assigned = False
    began = time.monotonic()
    timed_out = False
    exit_code = None
    operation_error = None
    cleanup_error = None
    cleanup = {"complete": False, "cleanup_timeout_seconds": 5,
               "job_termination_requested": False, "child_wait_complete": False,
               "job_handle_closed": False, "process_handles_closed": False,
               "stdio_handles_closed": False}

    def accounting():
        value = BASIC_ACCOUNTING()
        if not kernel.QueryInformationJobObject(job, 1, ctypes.byref(value), ctypes.sizeof(value), None):
            raise ctypes.WinError(ctypes.get_last_error())
        return value

    try:
        limits = EXTENDED_LIMIT()
        limits.BasicLimitInformation.LimitFlags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if not kernel.SetInformationJobObject(job, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
            raise ctypes.WinError(ctypes.get_last_error())
        environment = os.environ.copy()
        environment.pop("NoDefaultCurrentDirectoryInExePath", None)
        env_block = ctypes.create_unicode_buffer("\0".join(
            f"{key}={value}" for key, value in sorted(environment.items(), key=lambda row: row[0].upper())) + "\0\0")
        with log.open("wb") as output, open(os.devnull, "rb") as input_stream:
            try:
                out_handle = msvcrt.get_osfhandle(output.fileno())
                in_handle = msvcrt.get_osfhandle(input_stream.fileno())
                os.set_handle_inheritable(out_handle, True)
                os.set_handle_inheritable(in_handle, True)
                startup = STARTUPINFO()
                startup.cb = ctypes.sizeof(startup)
                startup.dwFlags = 0x100  # STARTF_USESTDHANDLES
                startup.hStdInput = in_handle
                startup.hStdOutput = out_handle
                startup.hStdError = out_handle
                text = ctypes.create_unicode_buffer(subprocess.list2cmdline(command))
                # Suspended, hidden, below normal priority, explicit Unicode environment.
                flags = 0x4 | 0x08000000 | 0x4000 | 0x400
                if not kernel.CreateProcessW(command[0], text, None, None, True, flags,
                                             env_block, str(cwd), ctypes.byref(startup), ctypes.byref(info)):
                    raise ctypes.WinError(ctypes.get_last_error())
                started = True
                if not kernel.AssignProcessToJobObject(job, info.hProcess):
                    raise ctypes.WinError(ctypes.get_last_error())
                assigned = True
                if kernel.ResumeThread(info.hThread) == 0xFFFFFFFF:
                    raise ctypes.WinError(ctypes.get_last_error())
                wait_result = kernel.WaitForSingleObject(info.hProcess, timeout_seconds * 1000)
                timed_out = wait_result == 258
                if wait_result not in (0, 258):
                    raise RuntimeError(f"owned child wait failed: wait={wait_result}")
            except BaseException as error:
                operation_error = error
            finally:
                # Keep the redirected handles open until every owned process has
                # stopped. No transcript hash is computed from this scope.
                try:
                    if started and assigned:
                        before = accounting()
                        cleanup["job_total_processes"] = before.TotalProcesses
                        cleanup["job_active_processes_before_cleanup"] = before.ActiveProcesses
                        if before.ActiveProcesses:
                            if not kernel.TerminateJobObject(job, 0xE0420002):
                                raise ctypes.WinError(ctypes.get_last_error())
                            cleanup["job_termination_requested"] = True
                        deadline = time.monotonic() + 5
                        while accounting().ActiveProcesses:
                            if time.monotonic() >= deadline:
                                raise RuntimeError("owned job still has active processes after cleanup bound")
                            time.sleep(0.025)
                        cleanup["job_active_processes_after_cleanup"] = 0
                    elif started:
                        # Assignment failed while the child was still suspended;
                        # it cannot have spawned any descendant.
                        if kernel.WaitForSingleObject(info.hProcess, 0) != 0:
                            if not kernel.TerminateProcess(info.hProcess, 0xE0420002):
                                raise ctypes.WinError(ctypes.get_last_error())
                            if kernel.WaitForSingleObject(info.hProcess, 5000) != 0:
                                raise RuntimeError("unassigned suspended child did not stop within cleanup bound")
                    if started:
                        if kernel.WaitForSingleObject(info.hProcess, 0) != 0:
                            raise RuntimeError("owned child is not signalled after cleanup")
                        cleanup["child_wait_complete"] = True
                        code = wintypes.DWORD()
                        if not kernel.GetExitCodeProcess(info.hProcess, ctypes.byref(code)):
                            raise ctypes.WinError(ctypes.get_last_error())
                        exit_code = code.value
                except BaseException as error:
                    cleanup_error = error
    finally:
        # Kill-on-close remains a fallback even when explicit cleanup failed.
        # A failed cleanup never reaches the stable-transcript hashing below.
        cleanup["job_handle_closed"] = bool(kernel.CloseHandle(job))
        handles_closed = True
        if info.hThread:
            handles_closed = bool(kernel.CloseHandle(info.hThread)) and handles_closed
        if info.hProcess:
            handles_closed = bool(kernel.CloseHandle(info.hProcess)) and handles_closed
        cleanup["process_handles_closed"] = handles_closed

    # Both redirected streams have left their with-scope; job and process/thread
    # handles have also closed. All owned descendants were verified inactive.
    cleanup["stdio_handles_closed"] = True
    cleanup["complete"] = (cleanup_error is None and cleanup["job_handle_closed"] and
        cleanup["process_handles_closed"] and (not started or cleanup["child_wait_complete"] and
        (not assigned or cleanup.get("job_active_processes_after_cleanup") == 0)))
    if cleanup_error is not None or not cleanup["complete"]:
        raise RuntimeError(f"owned-child cleanup failed; transcript hash withheld: "
                           f"{cleanup_error}; operation={operation_error}; {cleanup}")
    if operation_error is not None:
        raise operation_error
    transcript_sha = sha256(log)
    return {"pid": info.dwProcessId, "exit_code": exit_code,
            "timed_out": timed_out, "timeout_seconds": timeout_seconds,
            "wall_ms": round((time.monotonic() - began) * 1000),
            "owned_job": True, "cleanup": cleanup, "log": str(log),
            "log_raw_sha256": transcript_sha, "log_sha256": transcript_sha,
            "log_bytes": log.stat().st_size, "transcript_hash_after_cleanup": True}


def classify(row: dict, text: str, expected: dict, ids: list[str]) -> bool:
    summaries = SUMMARY.findall(text)
    checks = re.findall(r"^CHECK (\S+) (PASS|FAIL)$", text, re.MULTILINE)
    failures = [name for name, status in checks if status == "FAIL"]
    valid = (len(summaries) == 1 and tuple(map(int, summaries[0])) ==
             (expected["checks"], expected["failures"], expected["assertions"]) and
             [name for name, _ in checks] == ids and len(failures) == expected["failures"] and
             row["run"]["exit_code"] == (1 if expected["failures"] else 0) and
             not row["run"]["timed_out"])
    if expected.get("failure_id"): valid = valid and failures == [expected["failure_id"]]
    if expected.get("assert_substring"):
        lines = re.findall(r"^LEGACY_ASSERT (.*)$", text, re.MULTILINE)
        valid = valid and len(lines) == 1 and expected["assert_substring"] in lines[0]
    row.update({"numeric_summaries": summaries, "observed_check_ids": [name for name, _ in checks],
                "observed_failure_ids": failures, "expected": expected,
                "status": "pass" if valid and not expected["failures"] else
                          "expected_failure_observed" if valid else "fail"})
    return valid


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    b5 = Path(__file__).resolve().parents[1]
    workflow = args.workflow_root.resolve()
    out = args.out.resolve()
    if out.is_relative_to(b5): parser.error("artifact directory must be outside B5 source")
    if out.exists() and any(out.iterdir()): parser.error("fresh/empty artifact directory required")
    out.mkdir(parents=True, exist_ok=True)
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    result = {"schema_version": 1, "fixture": TAG, "executed": False, "success": False,
              "b5_root": str(b5), "workflow_root": str(workflow), "artifact_root": str(out),
              "variants": {}}
    try:
        result["sources"] = source_receipt(b5, workflow)
        if result["sources"]["mismatches"] or not all(r["matches_original_git_lf_blob"] for r in result["sources"]["baseline_evidence"]):
            raise ValueError("source content or exact original baseline mismatch")
        plan = json.loads((b5 / PLAN_PATH).read_text("utf-8"))
        if (plan["expected_candidate_checks"] != EXPECTED_CHECKS or plan["expected_candidate_failures"] != 0 or
            len(plan["candidate_check_ids"]) != EXPECTED_CHECKS or len(set(plan["candidate_check_ids"])) != EXPECTED_CHECKS or
            plan["baseline_api"] != {"checks": 9, "failures": 1, "assertions": 0, "failure_id": "default_actual_api_success"} or
            plan["baseline_consumer"] != {"checks": 3, "failures": 1, "assertions": 1,
                "failure_id": "legacy_consumer_selected_user_settings_callback", "assert_substring": "problem reading user settings"}):
            raise ValueError("numeric/control test plan changed")
        result["test_plan"] = plan
        if not args.run:
            result["status"] = "prepared_only"
            return 0
        compiler, result["toolchain"] = toolchain_receipt()
        flags = content_lines(workflow / "tools/build/msvc_flags.txt")
        includes = ["/I" + str((workflow / p).resolve()) for p in content_lines(workflow / "tools/build/msvc_includes.txt")]
        fixture = b5 / "tests/SystemUserProfilePC.cpp"
        string_source = b5 / "src/GameShared/GameClasses/Core/CgsStringUtils.cpp"
        string_lf = string_source.read_bytes().replace(b"\r\n", b"\n").decode("utf-8-sig")
        exact_definition = definition(string_lf, "void CgsCore::SPrintf(").encode("utf-8")
        if hashlib.sha256(exact_definition).hexdigest() != SPRINTF_DEFINITION_SHA256:
            raise ValueError("canonical SPrintf definition changed")
        result["executed"] = True
        for name, provider, consumer in (("candidate", PROVIDER, CONSUMER),
                                         ("baseline", LEGACY_PROVIDER, LEGACY_CONSUMER)):
            dest = out / name; dest.mkdir()
            (dest / "SPrintfDefinition.inc").write_bytes(exact_definition)
            sprintf = dest / "SPrintfProvider.cpp"
            sprintf.write_text('#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
                '#include <cstdarg>\n#include <stdio.h>\n#include "SPrintfDefinition.inc"\n', encoding="utf-8", newline="\n")
            sources = [fixture, b5 / provider, b5 / consumer,
                b5 / "src/GameShared/GameClasses/System/PC/CgsPcNetIdentity.cpp",
                b5 / "src/GameShared/GameClasses/System/X360/CgsXOverlappedX360.cpp", sprintf]
            exe = dest / "regression.exe"
            command = [str(compiler), *[a for line in flags for a in line.split()],
                       "/D_ALLOW_KEYWORD_MACROS=1", *includes, *map(str, sources),
                       "/Fe:" + str(exe), "/link", "/OPT:REF", "user32.lib", "gdi32.lib"]
            row = {"source_paths": list(map(str, sources)), "compile_command": command,
                   "generated_provider": identity(sprintf),
                   "exact_sprintf_definition": identity(dest / "SPrintfDefinition.inc")}
            result["variants"][name] = row
            row["compile"] = run_owned(command, dest, dest / "compile.log", 120)
            if row["compile"]["exit_code"] != 0 or row["compile"]["timed_out"] or not exe.is_file():
                row["status"] = "compile_failed"; continue
            row["executable_sha256"] = sha256(exe); row["executable_bytes"] = exe.stat().st_size
            modes = [("candidate", [], {"checks": plan["expected_candidate_checks"], "failures": 0, "assertions": 0}, plan["candidate_check_ids"])] if name == "candidate" else [
                ("baseline_api", ["--baseline-api"], plan["baseline_api"], plan["candidate_check_ids"][:9]),
                ("baseline_consumer", ["--baseline-consumer"], plan["baseline_consumer"], [
                    "legacy_consumer_selected_user_settings_callback", "legacy_consumer_preserves_signin_name_before_settings", "legacy_consumer_actual_error87_assert"])]
            mode_rows = {}
            for mode, extra, expected, check_ids in modes:
                run_row = {"run": run_owned([str(exe), *extra], dest, dest / (mode + ".log"), 25)}
                mode_rows[mode] = run_row
                classify(run_row, (dest / (mode + ".log")).read_text("utf-8", errors="replace"), expected, check_ids)
            row["modes"] = mode_rows
            row["status"] = "pass" if all(r["status"] in ("pass", "expected_failure_observed") for r in mode_rows.values()) else "fail"
        result["success"] = len(result["variants"]) == 2 and all(r["status"] == "pass" for r in result["variants"].values())
        result["status"] = "pass" if result["success"] else "fail"
        return 0 if result["success"] else 1
    except Exception as error:
        result["status"] = "error"; result["error"] = str(error)
        return 1
    finally:
        result["python"] = sys.version
        # run_owned returns only after owned descendants and output handles close.
        # No transcript byte hash is accepted before its cleanup receipt is complete.
        (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
        print(json.dumps({"status": result.get("status"), "success": result["success"], "receipt": str(out / "result.json")}))


if __name__ == "__main__":
    raise SystemExit(main())
