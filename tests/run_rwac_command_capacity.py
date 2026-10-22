"""Run the pinned RWAC capacity fixture with an owned, bounded x64 MSVC child.

Use --run in hosted CI. Without it, prepare a source/toolchain receipt only.
All writes go to a fresh --out artifact directory outside the B5 checkout.
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
import shlex
import shutil
import subprocess
import sys
import time


BASELINE_FAILURES = {
    "overflow stops before fifth record",
    "overflow leaves cursor untouched",
    "overflow leaves full backing and canaries untouched",
    "overflow has explicit synchronous record",
    "record contains actual pending/capacity/stride/highwater/frame",
    "record is bounded and complete",
}
SUMMARY = re.compile(r"^RWAC_CAPACITY checks=(\d+) failures=(\d+)$", re.MULTILINE)
FIRST_PASS_SUMMARY = re.compile(r"^RWAC_FIRST_PASS checks=(\d+) failures=(\d+)$", re.MULTILINE)
SOURCE_PIN_PATH = "tests/evidence/rwac_command_capacity/source_pins.json"
NORMALIZATION = "crlf_to_lf_only"
PINNED_SOURCES = {
    "src/GameShared/GameClasses/System/PC/CgsAudioOutputPC.cpp",
    "src/GameShared/GameClasses/System/PC/CgsDacOutputPC.cpp",
    "src/GameShared/GameClasses/System/PC/CgsRwacCommandTracePC.h",
    "src/GameSource/Sound/Module/BrnRootSoundModule.cpp",
    "vendor/renderware/src/rw/audio/core/PlugIn.cpp",
    "tests/RwacCommandCapacity.cpp",
    "tests/run_rwac_command_capacity.py",
    "tests/evidence/rwac_command_capacity/PlugIn_b823.cpp",
}


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def content_identity(path: Path) -> dict:
    raw = path.read_bytes()
    # This is the entire content-normalization contract. Do not use splitlines,
    # text-mode decoding, stripping, tokenization or any other whitespace change.
    lf = raw.replace(b"\r\n", b"\n")
    pairs = raw.count(b"\r\n")
    bare_lf = raw.count(b"\n") - pairs
    bare_cr = raw.count(b"\r") - pairs
    variant = ("contains_bare_cr" if bare_cr else
               "mixed_crlf_lf" if pairs and bare_lf else
               "crlf" if pairs else "lf" if bare_lf else "no_newlines")
    return {"actual_raw_sha256": hashlib.sha256(raw).hexdigest(),
            "actual_raw_bytes": len(raw), "actual_lf_sha256": hashlib.sha256(lf).hexdigest(),
            "actual_lf_bytes": len(lf), "actual_raw_eol_variant": variant,
            "actual_raw_crlf_pairs": pairs, "actual_raw_bare_lf": bare_lf,
            "actual_raw_bare_cr": bare_cr}


def content_lines(path: Path) -> list[str]:
    return [line.strip() for line in path.read_text("utf-8").splitlines()
            if line.strip() and not line.lstrip().startswith("#")]


def checked_source_path(b5: Path, relative: str) -> Path:
    path = (b5 / Path(relative)).resolve()
    if not path.is_relative_to(b5) or path == b5:
        raise RuntimeError("source pin escapes the B5 checkout: " + relative)
    return path


def source_receipt(b5: Path, workflow: Path) -> dict:
    pins_path = b5 / SOURCE_PIN_PATH
    pins = json.loads(pins_path.read_text("utf-8"))
    if pins.get("schema_version") != 2 or pins.get("normalization") != NORMALIZATION:
        raise RuntimeError("unsupported source-pin schema")
    if set(pins.get("files", {})) != PINNED_SOURCES:
        raise RuntimeError("source pins must cover the exact production/test/control composition")
    rows = {}
    for relative, expected in pins["files"].items():
        if set(expected) != {"expected_lf_sha256", "expected_lf_bytes"} or \
                not isinstance(expected["expected_lf_bytes"], int) or expected["expected_lf_bytes"] < 0:
            raise RuntimeError("invalid LF-content pin fields: " + relative)
        if not re.fullmatch(r"[0-9a-f]{64}", expected["expected_lf_sha256"]):
            raise RuntimeError("invalid source SHA256: " + relative)
        path = checked_source_path(b5, relative)
        actual = content_identity(path)
        rows[relative] = actual | expected | {"matches_lf_content":
            actual["actual_lf_sha256"] == expected["expected_lf_sha256"] and
            actual["actual_lf_bytes"] == expected["expected_lf_bytes"]}
    mismatches = [name for name, row in rows.items() if not row["matches_lf_content"]]
    baseline = pins["baseline_evidence"]
    baseline_path = checked_source_path(b5, baseline["path"])
    baseline_lf = baseline_path.read_bytes().replace(b"\r\n", b"\n")
    baseline_blob = hashlib.sha1(b"blob " + str(len(baseline_lf)).encode("ascii") +
                                b"\0" + baseline_lf).hexdigest()
    baseline_receipt = baseline | rows[baseline["path"]] | {
        "actual_checkout_lf_git_blob_sha1": baseline_blob,
        "matches_original_git_lf_blob": baseline_blob == baseline["original_git_lf_blob_sha1"]}
    if not baseline_receipt["matches_original_git_lf_blob"] and baseline["path"] not in mismatches:
        mismatches.append(baseline["path"])
    canonical = {name: content_identity(workflow / name) for name in
                 ("tools/build/msvc_flags.txt", "tools/build/msvc_includes.txt")}
    return {"schema_version": 2, "normalization": NORMALIZATION, "base_head": pins["base_head"],
            "pins_file": content_identity(pins_path), "files": rows,
            "baseline_evidence": baseline_receipt, "mismatches": mismatches,
            "canonical_tool_files": canonical}


def toolchain_receipt() -> tuple[Path, dict]:
    found = shutil.which("cl")
    if not found:
        raise RuntimeError("cl.exe unavailable; run in a configured x64 MSVC environment")
    compiler = Path(found).resolve()
    linker = compiler.parent / "link.exe"
    if not linker.is_file():
        raise RuntimeError("the compiler's matching link.exe is unavailable")
    files = [compiler, linker]
    for name in ("c1xx.dll", "c2.dll", "mspdbcore.dll"):
        path = compiler.parent / name
        if path.is_file():
            files.append(path)
    # Paths/versions only; no credentials or unrelated environment are recorded.
    environment = {name: os.environ.get(name) for name in
                   ("VCToolsVersion", "VisualStudioVersion", "VSCMD_ARG_TGT_ARCH",
                    "WindowsSDKVersion", "CL", "_CL_")}
    if environment["CL"] or environment["_CL_"]:
        raise RuntimeError("CL/_CL_ compiler overrides must be unset for a pinned fixture run")
    return compiler, {"files": {str(path): sha256(path) for path in files},
                      "environment": environment}


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


def classify_run(row: dict, text: str, baseline: bool) -> None:
    summaries = SUMMARY.findall(text)
    failures = re.findall(r"^FAIL (.+)$", text, re.MULTILINE)
    first_pass_summaries = FIRST_PASS_SUMMARY.findall(text)
    first_pass_failures = re.findall(r"^FAIL_FIRST_PASS (.+)$", text, re.MULTILINE)
    row["failure_labels"] = failures
    row["checks"] = int(summaries[0][0]) if len(summaries) == 1 else None
    row["failures"] = int(summaries[0][1]) if len(summaries) == 1 else None
    row["first_pass_failure_labels"] = first_pass_failures
    row["first_pass_checks"] = int(first_pass_summaries[0][0]) if len(first_pass_summaries) == 1 else 0
    row["first_pass_failures"] = int(first_pass_summaries[0][1]) if len(first_pass_summaries) == 1 else 0
    numeric = (row["checks"] is not None and row["checks"] > 0 and
               row["failures"] == len(failures))
    first_pass_numeric = (len(first_pass_summaries) == 1 and
               row["first_pass_checks"] == 6 and
               row["first_pass_failures"] == len(first_pass_failures))
    if baseline:
        row["pass"] = (numeric and row["run"]["exit_code"] == 1 and
            not row["run"]["timed_out"] and set(failures) == BASELINE_FAILURES and
            len(failures) == len(BASELINE_FAILURES) and
            len(first_pass_summaries) == 0 and len(first_pass_failures) == 0)
    else:
        row["pass"] = (numeric and row["run"]["exit_code"] == 0 and
            not row["run"]["timed_out"] and row["failures"] == 0 and
            "TRACE_BURST " in text and "TRACE_STARVATION " in text and
            first_pass_numeric and row["first_pass_failures"] == 0)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workflow-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    b5 = Path(__file__).resolve().parents[1]
    workflow = args.workflow_root.resolve()
    out = args.out.resolve()
    if out.is_relative_to(b5) or out == workflow or workflow.is_relative_to(out):
        raise RuntimeError("--out must be an owned artifact directory outside the B5 source checkout")
    if out.exists() and any(out.iterdir()):
        raise RuntimeError("--out must be fresh or empty; existing artifacts are never replaced")
    out.mkdir(parents=True, exist_ok=True)
    result = {"state": "prepared", "runner": str(Path(__file__).resolve()),
              "source_checkout": str(b5), "workflow_root": str(workflow),
              "fixture_checks": 0, "fixture_failures": 0,
              "first_pass_checks": 0, "first_pass_failures": 0,
              "intentional_failure_controls": 0, "intentional_failure_checks": 0,
              "runs": []}
    try:
        result["sources"] = source_receipt(b5, workflow)
        if result["sources"]["mismatches"]:
            raise RuntimeError("candidate source pins differ: " +
                               ", ".join(result["sources"]["mismatches"]))
        compiler, result["toolchain"] = toolchain_receipt()
        fixture = b5 / "tests/RwacCommandCapacity.cpp"
        flags = [token for line in content_lines(workflow / "tools/build/msvc_flags.txt")
                 for token in shlex.split(line, posix=False)]
        includes = [workflow / Path(line) for line in
                    content_lines(workflow / "tools/build/msvc_includes.txt")]
        # Ensure the canonical workflow includes resolve to THIS candidate B5.
        if not includes or includes[0].resolve() != b5 / "src":
            raise RuntimeError("--workflow-root does not contain the candidate B5 checkout")
        for name, baseline in (("candidate", False), ("baseline_control", True)):
            exe = out / (name + ".exe")
            command = [str(compiler), *flags, "/D_ALLOW_KEYWORD_MACROS=1"]
            command += ["/we4473", "/we4477"]
            if baseline:
                command += ["/DBRN_RWAC_BASELINE_CONTROL=1", "/DBRN_RWAC_BASELINE_PRODUCER=1"]
            command += ["/I" + str(path) for path in includes]
            command += [str(fixture), "/Fo" + str(out / (name + ".obj")),
                        "/Fe" + str(exe), "/Fd" + str(out / (name + ".pdb")),
                        "/link", "/OPT:REF", "/INCREMENTAL:NO"]
            row = {"name": name, "intentional_failure_control": baseline, "command": command}
            result["runs"].append(row)
            if not args.run:
                continue
            row["compile"] = run_owned(command, out, out / (name + "_compile.log"), 120)
            if row["compile"]["timed_out"] or row["compile"]["exit_code"] != 0 or not exe.is_file():
                row["pass"] = False
                continue
            row["fixture_exe_sha256"] = sha256(exe)
            row["fixture_exe_bytes"] = exe.stat().st_size
            row["run"] = run_owned([str(exe)], out, out / (name + "_run.log"), 20)
            text = Path(row["run"]["log"]).read_text("utf-8", errors="replace")
            classify_run(row, text, baseline)
            if baseline:
                result["intentional_failure_controls"] += int(row["pass"])
                result["intentional_failure_checks"] += row["failures"] or 0
            else:
                result["fixture_checks"] += row["checks"] or 0
                result["fixture_failures"] += row["failures"] or 0
                result["first_pass_checks"] += row["first_pass_checks"] or 0
                result["first_pass_failures"] += row["first_pass_failures"] or 0
        if args.run:
            result["state"] = "pass" if (len(result["runs"]) == 2 and
                all(row.get("pass") for row in result["runs"])) else "fail"
    except Exception as error:
        result["state"] = "fail"
        result["error"] = f"{type(error).__name__}: {error}"
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: result[key] for key in
        ("state", "fixture_checks", "fixture_failures", "first_pass_checks", "first_pass_failures",
         "intentional_failure_controls", "intentional_failure_checks")} | {"result": str(out / "result.json")}))
    return int(result["state"] == "fail")


if __name__ == "__main__":
    sys.exit(main())
