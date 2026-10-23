"""Owned bounded child helper copied exactly from run_system_user_profile_pc.py. No work at import."""
import ctypes
from ctypes import wintypes
import hashlib
import os
from pathlib import Path
import subprocess
import time

def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

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

