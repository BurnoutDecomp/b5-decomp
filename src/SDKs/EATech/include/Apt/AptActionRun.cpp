// ===========================================================================
// EATech Apt -- AptActionInterpreter::runStream, the ActionScript execution loop.
//   DECOMPILED from the PS3 EXTERNAL ELF @0x81BD50 (cross-checked vs X360 ARTIST
//   @0x82ADD440, whose dispatch confirmed sGlobalTable @ vaddr 0x82F73068).
//
// Runs an action bytecode stream against a CIH scope:
//   * top-level run (nLength == -1): push the CIH onto the target/CIH stack;
//   * build the LocalContextT (PC + CIH + scope + character instance);
//   * reserve a fresh operand-stack base (mnStackBase = current top) so the run's
//     temporaries can be unwound cleanly;
//   * DISPATCH LOOP: until a stop op (ctx.mbStop), the stream end (bounded mode),
//     or an abort (mnAbortValue): read the opcode byte, advance the PC, then call
//     sGlobalTable[opcode](this, &ctx); between ops, drop the pending temp once the
//     PC reaches its release marker;
//   * running off the end of a bounded stream pushes `undefined` as the result;
//   * UNWIND: release the operands left above the run's base -- all of them for a
//     top-level run, all-but-one for a bounded sub-stream (which returns its top
//     value); then restore the saved base and (top-level) pop the CIH stack.
// Returns the final program counter.
//
// x64: the PC is a real 64-bit pointer into the loaded action stream; nLength is a
// byte count, so `pStream + nLength` (the end) and the operand indexing are
// pointer-width-correct without a transcode.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptCIH.h"   // AddRef/Release on the CIH

// FLAG (wired at AptInit; see AptValueConvert.cpp).
extern AptValue* gpUndefinedValue;

// FLAG (string-pool key): the scope ("this") variable name runStream binds at entry
// (console stru_1059C8A0). Homed with the StringPool constants. getVariable's body
// is deferred too (see AptActionInterpreter.h), so this run-scope bind is a FLAG'd
// path until both land -- it does not affect the dispatch loop itself.
extern const EAStringC* gpAptThisKey;

const unsigned char* AptActionInterpreter::runStream(
    const unsigned char* pStream, AptCIH* pCIH, int nLength, AptCharacterInst* pCharInst)
{
    const bool bTopLevel = (nLength == -1);

    // Top-level run: push the running CIH onto the target/CIH stack.
    // (The entry top is snapshotted so the exit pop releases EXACTLY the pushed CIH
    // and restores the entry depth -- observably identical to the console's push/pop
    // when the nested handlers stay balanced, and it self-heals when an
    // un-homed nested path drifts the top; see the exit pop.)
    const int nSavedCIHTop = mnCIHStackTop;
    if (bTopLevel)
    {
        mpCIHStack[mnCIHStackTop] = pCIH;
        ++mnCIHStackTop;
        pCIH->AddRef();
    }

    // The per-run execution context.
    LocalContextT ctx;
    ctx.mpProgramCounter      = pStream;
    ctx.mpPendingReleaseValue = 0;
    ctx.mpPendingReleasePC    = 0;
    ctx.mpCIH                 = pCIH;
    ctx.mpScopeVariable       = getVariable(pCIH, 0, gpAptThisKey, 1, 1, 0);  // FLAG: getVariable deferred
    ctx.mbStop                = false;
    ctx.mpCharacterInst       = pCharInst;

    // Reserve a fresh operand-stack base for this run (nested runs save/restore it).
    const int  nSavedBase = mnStackBase;
    const bool bRun       = (mpAbortValue == 0);
    mnStackBase = mnStackTop;

    bool bRanOffEnd = false;
    if (bRun)
    {
        const unsigned char* const pEnd = pStream + nLength;  // only meaningful when bounded
        while (true)
        {
            // Drop the pending temporary once the PC reaches its release marker.
            // (Null-guarded: a handler on a not-yet-complete x64 path can set the PC
            // marker with no value; the console always pairs them. FLAG hardening.)
            if (ctx.mpPendingReleasePC == ctx.mpProgramCounter && ctx.mpProgramCounter)
            {
                if (ctx.mpPendingReleaseValue)
                    ctx.mpPendingReleaseValue->Release();
                ctx.mpPendingReleaseValue = 0;
                ctx.mpPendingReleasePC    = 0;
            }
            if (ctx.mbStop)
                break;

            const unsigned char opcode = *ctx.mpProgramCounter;
            ++ctx.mpProgramCounter;

            // Bounded run that has walked past its end -> result is `undefined`.
            if (!bTopLevel && ctx.mpProgramCounter > pEnd)
            {
                bRanOffEnd = true;
                break;
            }

            sGlobalTable[opcode](this, &ctx);

            if (mpAbortValue)
                break;
        }
    }

    if (bRanOffEnd)
    {
        // Push the `undefined` result the off-end exit leaves behind.
        mpStack[mnStackTop] = gpUndefinedValue;
        ++mnStackTop;
        gpUndefinedValue->AddRef();
    }

    // Unwind the operands left above the run's base. A top-level (unbounded) run
    // releases all of them; a bounded sub-stream leaves its top value as the result.
    const int nFloor = bTopLevel ? mnStackBase : (mnStackBase + 1);
    while (mnStackTop > nFloor)
    {
        // Null-guarded: the console never leaves a raw null on the operand stack
        // (its miss paths push the `undefined` singleton), but a not-yet-complete
        // x64 handler path can (e.g. a raw getVariable-result push) -- skip it
        // rather than AV in Release. FLAG hardening; observably identical on
        // valid streams.
        if (mpStack[mnStackTop - 1])
            mpStack[mnStackTop - 1]->Release();
        --mnStackTop;
    }

    mnStackBase = nSavedBase;

    // Top-level run: pop the CIH off the target stack. Release the exact CIH this
    // run pushed (pCIH) and restore the entry depth rather than dereferencing an
    // unwritten slot on a drifted top (the 0xbaadf00d AV this replaces -- see the
    // entry note).
    if (bTopLevel)
    {
        pCIH->Release();
        mnCIHStackTop = nSavedCIHTop;
    }

    // FLAG: the console then flushes the GC deferred-release vector
    // (gpValuesToRelease) when the operand stack is empty (or holds only the
    // undefined result). Deferred with the GC layer; does not affect control flow.

    return ctx.mpProgramCounter;
}
