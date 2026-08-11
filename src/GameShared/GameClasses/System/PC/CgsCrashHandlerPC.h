#pragma once

// CgsSystem::CrashHandler - HOST-ONLY (PC) last-chance exception reporter. No X360
// counterpart: the console halts into the debug monitor on an unhandled exception; on the
// PC the process would die with only the Windows error dialog, losing the state the log
// captures for asserts. Install() wires a SetUnhandledExceptionFilter (+ SIGABRT hook)
// that writes the exception, registers and a map-resolved call-stack to the unified game
// log (BrnGame.log) in the same "Callstack:/EndCallstack" shape as the assert path, and
// saves a screenshot of the game window (BrnCrash.png next to the exe) before the default
// handling continues.

namespace CgsSystem
{
namespace CrashHandler
{
    void Install();
}
}
