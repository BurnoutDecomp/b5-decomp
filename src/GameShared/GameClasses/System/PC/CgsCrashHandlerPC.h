#pragma once

// CgsSystem::CrashHandler - HOST-ONLY (PC) last-chance exception reporter. No X360
// counterpart: the console halts into the debug monitor on an unhandled exception; on the
// PC the process would die with only the Windows error dialog, losing the state the log
// captures for asserts. Install() wires a SetUnhandledExceptionFilter (+ SIGABRT hook)
// that writes the exception, registers and a map-resolved call-stack to the unified game
// log (BrnGame.log) in the same "Callstack:/EndCallstack" shape as the assert path, and
// saves a screenshot of the game window (BrnCrash.png next to the exe) before the default
// handling continues.

// ⭐ 2026-08-20 (gateui round 7, defect B -- "the process leaves mid-drive and writes NOTHING").
// SetUnhandledExceptionFilter is NOT a complete death net on the PC. Four ways this process can
// die WITHOUT that filter ever running, all of them silent in BrnGame.log:
//   (a) __fastfail  -- `int 29h` goes straight to the kernel; no VEH, no UEF, no unwind. The CRT
//       reaches it from `_invalid_parameter` -> `_invoke_watson` (a bad argument to any *_s /
//       CRT routine) and from `__report_gsfailure` (a /GS stack-cookie mismatch, i.e. a stack
//       buffer overrun). Both are LINKED INTO THIS EXE (Burnout_PC.map: libucrt:invalid_parameter.obj,
//       LIBCMT:gs_report.obj).
//   (b) EXCEPTION_STACK_OVERFLOW -- the filter is entered on the one page left below the consumed
//       guard page, WriteReport + the GDI+ screenshot need far more than that, and the resulting
//       second fault inside the filter kills the process outright with no report.
//   (c) a deliberate process exit from anywhere (a vendor panic path, a bring-up early-out).
//   (d) std::terminate / a pure-virtual call, which funnel to abort()'s reportfault leg.
// Install() now closes (a) partially (the invalid-parameter leg, which is the hookable one),
// (b) for THE THREAD THAT CALLS Install() via SetThreadStackGuarantee, (c) partially via atexit,
// and (d) via explicit handlers -- plus a vectored handler that records the FIRST-CHANCE
// exception, so even a fault someone else swallows leaves a line.
//
// ⚠️ ROUND-8 CORRECTION -- READ THIS BEFORE RULING A ROUTE OUT FROM A MISSING LINE.
//   * (c) IS NOT FULLY CLOSED. atexit-registered functions run for exit() and for a normal
//     return out of main/WinMain -- and for NOTHING ELSE. `_exit`, `_Exit`, `quick_exit`,
//     `ExitProcess`, `TerminateProcess` and `__fastfail` all bypass the onexit table entirely.
//     The shutdown watchdog in CgsHardwareInitPC.cpp is itself a TerminateProcess. So the
//     ABSENCE of an "[exit-diag] CRT exit reached" line does NOT mean "it was not an exit path";
//     for that half of the family the ONLY identification is the recorded process exit code
//     (tools\diagnostics\flow_run.ps1's EXIT line).
//   * (b) is per-thread. SetThreadStackGuarantee affects only the calling thread and the poll
//     measures only whichever thread calls it, but a fault or overflow on the shutdown watchdog
//     thread, an EAThread worker, or an XAudio2/D3D9 internal thread ends the process just the
//     same.
//   * DEFECT B ("the process leaves mid-drive and writes nothing") IS OPEN, NOT FIXED. Nothing
//     here repairs it; this is instrumentation only. As of round 8 not one of these tripwires
//     has ever fired: the exit did not recur across 11 post-delta runs, and survival predates
//     two of the three legs, so those runs are a NON-REPRODUCTION and must not be read as a
//     repair. The round-7 leading hypothesis (a per-drive leak -> OOM) is WEAKENED by this
//     file's own footprint data, which PLATEAUS (~1.89 GB private bytes, flat for the last
//     ~90 s of a 275 s run) while the one run that died did so inside the growth phase at a
//     footprint the survivors passed straight through. Process private bytes cannot see a
//     system-commit kill; that measurement (GlobalMemoryStatusEx ullTotalPageFile /
//     ullAvailPageFile in the same 10 s line) is the open follow-up.
// What remains genuinely unhookable is the /GS + heap-corruption __fastfail; those are
// identified from the PROCESS EXIT CODE instead (0xC0000409 / 0xC0000374), which
// tools\diagnostics\flow_run.ps1 now records.
//
// PollProcessHealth() (⭐ round 8: this banner and the <intrin.h> include note both used to call
// it "PollStackHighWater", which is not a function that exists) carries the companion tripwire
// for (b): the x64 TEB's StackLimit only ever moves DOWN (a stack page stays committed once
// touched), so StackBase - StackLimit is a free high-water mark of this thread's peak COMMITTED
// stack -- page-quantised, never below the PE's initial commit, and raised by the 128 KB
// SetThreadStackGuarantee that Install() sets before anything polls, so it is a bound on stack
// use, not a measurement of it. It is also UNCALIBRATED: every run so far prints the same
// `160 KB of 1024 KB reserved` at log line 16 and never steps again in 275 s of driving, which
// is equally consistent with "driving is genuinely shallower than boot" and with "the reading
// has no demonstrated sensitivity". Do not rule stack overflow out on this number until a
// deliberate deep call has been shown to move it. Called once per frame from the engine loop;
// it logs only when the mark grows past its previous 32 KB step, so a run that is nowhere near
// the 1 MB reserve costs one gs-relative read a frame and prints nothing.

namespace CgsSystem
{
namespace CrashHandler
{
    void Install();

    // Cheap per-frame health poll (see the banner above). Callable from any thread (⭐ round 8:
    // "safe" was overstated -- CgsLog's line buffer is a lock-free static char[2048] + length, so
    // concurrent writers interleave; the poll's own state is per-thread, the LOGGING is not); each
    // thread tracks its own stack mark. Three legs, cheapest first:
    //   * the stack high-water tripwire -- one gs-relative read, logs only on a new 32 KB step;
    //   * a private-bytes / working-set line every 10 s, which is what settles "is the drive
    //     leaking?" with a number instead of a glance at the on-screen counter;
    //   * an OPT-IN heap integrity sweep, BRN_HEAP_CHECK=<seconds> (default off: HeapValidate
    //     walks every block and can stall for a noticeable fraction of a second). A run that
    //     ends in a silent __fastfail with a "heap check FAILED" line before it has been
    //     localised to a frame, a position and a bundle load.
    void PollProcessHealth();
}
}
