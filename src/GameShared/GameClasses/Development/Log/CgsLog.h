#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// The engine's debug-print/log front-end. Game code logs through CgsDev::Log::gpDebugPrint
// (a StrStreamBase: `*gpDebugPrint << "text" << value << "\n"`), gated by
// CgsDev::Message::gxMessageFilterFlags. Every log line - debug prints, channel output, and
// asserts - is routed to a single game log file by WriteToLog. Reconstructed from the DecFIGS
// DWARF (Development/Log/*) + the X360 call sites; the original fanned output to console /
// on-screen channels, here unified into one log file (+ the debugger output).
namespace CgsDev
{
namespace Log
{
    // Central sink: append one chunk of text to the game log file (and the debugger output).
    void WriteToLog(const char* lpcText);

    // The debug-print stream: a StrStreamBase whose char* sink forwards to WriteToLog, so the
    // engine's `gpDebugPrint << ...` logging lands in the log file.
    struct DebugPrint : public StrStreamBase
    {
        // Un-hide the inherited scalar formatting overloads (s32/u32/u64/f32/void*/PrintMode):
        // overriding operator<<(const char*) below would otherwise hide every base-class
        // overload by name -- the same fix CgsStrStream.h's StrStream carries, needed here
        // since 2026-08-06 when a log chain first STARTS with a scalar
        // (PhysicsSimulationModule::AddContactSpiesToOutputQueue's contact-dump lines).
        using StrStreamBase::operator<<;

        StrStreamBase& operator<<(const char* lpcText) override;
    };

    extern DebugPrint* gpDebugPrint;
}

namespace Message
{
    // Log-category filter. Call sites gate logging on `(gxMessageFilterFlags & bit)`; set broad
    // so the engine's logging is captured. (The real per-category default differs; tune later.)
    // Width is 64-bit: the X360 asm loads/stores the full 64-bit value and the DecFIGS DWARF
    // (CgsMessage.h:40) types it `uint64_t`. Matches CgsMessage.h's `extern FilterFlag (u64)`
    // declaration of the same CgsDev::Message::gxMessageFilterFlags symbol (no ODR conflict).
    extern u64 gxMessageFilterFlags;
}
}
