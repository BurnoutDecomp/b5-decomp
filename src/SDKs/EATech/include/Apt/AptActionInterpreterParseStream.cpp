// ===========================================================================
// EATech Apt -- AptActionInterpreter::_parseStream: the action-stream operand
// resolver (and its inverse).
//
//   DECOMPILED from the Xbox One build (the x64-primary Apt rung):
//     _parseStream == XB1 sub_14084A920 (~466 pseudocode lines), verified against
//     the assembly dump (parse tree, record strides, the float-case branch).
//
// One pass over an action bytecode stream: `op = *pc++`, opcode 0x00 terminates.
// For every operand-carrying opcode, resolve its serialized inline operands IN
// PLACE (file-relative offsets -> live pointers, constant-record indices -> live
// AptValue*s) -- or, with a null parse context, do the exact inverse (un-rebase +
// release) for the movie-unload path. Until a stream has been through this parser
// it must NOT be dispatched: runStream's handlers read the PARSED form.
//
// Direction: pConstCtx != 0 -> RESOLVE (+nBase; constants built);
//            pConstCtx == 0 -> UNRESOLVE (-nBase; constants released/re-indexed).
//
// The GUIAPT64 stream operand records are 8-ALIGNED with 8-byte pointer slots
// (the XB1 asm: `(pc+7) & ~7`, qword loads) -- unlike the 4-aligned console form.
//
// The parse CONTEXT (pConstCtx) is the movie's "Apt constant file" CHUNK (threaded
// CompleteLoad -> Fixup -> resolve64, the xb1 a4/a3/a3 chain): its record table
// pointer lives at ctx+0x28 [xb1:+40] (itemStart, relocated to absolute around
// Fixup by Resolve), records are 16-byte {u32 type + pad, 8-byte payload}, and
// string payloads are ctx-relative. Layout == libapt2 Const::Write (verified).
// The const-chunk header is a serialised .apt blob, so the ctx is walked with the
// documented serialised-blob offset idiom (the same exception Fixup/resolve64 use;
// AptConstFile.h still models only the 32-bit header fields).
//
// Constant-record types (the Push/DefineDictionary entry resolution):
//   1 string   : payload = ctx-relative offset -> rebase, AptString::Create
//                (interned; arrives with its own counted ref), un-rebase back.
//   6 float    : 0.0f collapses to the pooled AptInteger::Create(0) (the XB1
//                vucomiss branch -- faithful quirk); else AptFloat::Create(f).
//   7 integer  : AptInteger::Create(payload).
//   8 lookup   : &AptLookup pool[idx] when the pool holds idx, else null.
//   5 boolean  : the shared AptBoolean singleton (Create(payload != 0)).
//   4 register : &AptRegister file[idx] when the file holds idx, else null.
//   3          : the shared `undefined` singleton.
//   (other)    : null.
// Each resolved value is written into the stream's entry slot and AddRef'd --
// UNLESS it is a defined string (Create's ref transfers). Every 16 entries, and
// after every resolve-direction opcode, the GC deferred-release vector drains.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"       // AptString::Create
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"      // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"        // AptFloat::Create
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"      // AptBoolean::Create (the shared pair)
#include "SDKs/EATech/include/Apt/AptValue/AptLookup.h"       // AptLookup::GetPoolEntry/PoolHolds
#include "SDKs/EATech/include/Apt/AptValue/AptRegister.h"     // AptRegister::GetFileEntry/FileHolds
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"  // gValuesToRelease drain

#include <cstdint>

extern AptValue*          gpUndefinedValue;    // off_8324D814 / xb1 qword_14147A010
extern AptGCReleaseVector gValuesToRelease;    // off_8324E51C / xb1 qword_14147A410

// The per-string return to the temporary string pool (xb1 sub_14083F2A0), homed in
// Apt/AptStringPool.cpp (the FindOrCreate hit-path inverse); reached only on movie
// UNLOAD (the unresolve direction).
extern void AptStringPoolReleaseString(AptString* pString);

// The pool-string INTERNER (xb1 sub_140838AE0 == StringPool::FindOrCreate; free
// forwarder in AptStringPool.cpp -- the full StringPool.h collides with
// AptNativeHash.h's mini StringPool in this TU's include set). Every serialized
// constant string resolves through it: the interned node is chain-AddRef'd +
// GC-rooted, which is what survives the per-op deferred-release drains below --
// a plain temp AptString::Create dies at the next drain when its node was a
// recycled allocation (the 'BuildName' pool-string death, 2026-07-09).
extern AptString* AptStringPool_FindOrCreate(const char* pName);

namespace
{
    // The 8-aligned operand cursor (xb1: (pc + 7) & ~7).
    inline unsigned char* Align8(unsigned char* p)
    {
        return reinterpret_cast<unsigned char*>(
            (reinterpret_cast<uintptr_t>(p) + 7) & ~static_cast<uintptr_t>(7));
    }

    inline uintptr_t& Slot(unsigned char* p, int nOff = 0)
    {
        return *reinterpret_cast<uintptr_t*>(p + nOff);
    }
    inline int64_t& Slot64(unsigned char* p, int nOff = 0)
    {
        return *reinterpret_cast<int64_t*>(p + nOff);
    }

    // Typed reads of a serialized constant/DefineFunction record field (the const-chunk
    // header is not a recovered type -- the documented serialised-blob idiom, per the file note).
    inline int32_t  RecI32(unsigned char* p, int nOff) { return *reinterpret_cast<const int32_t*>(p + nOff); }
    inline uint32_t RecU32(unsigned char* p, int nOff) { return *reinterpret_cast<const uint32_t*>(p + nOff); }
    inline float    RecF32(unsigned char* p, int nOff) { return *reinterpret_cast<const float*>(p + nOff); }

    // ±base with the null-stays-null guard every xb1 slot rebase carries.
    inline void Rebase(uintptr_t& rSlot, uintptr_t nBase, bool bUnresolve)
    {
        if (rSlot != 0)
            rSlot = bUnresolve ? (rSlot - nBase) : (rSlot + nBase);
        else
            rSlot = 0;
    }

    // Resolve one serialized constant record (16-byte {u32 type + pad, 8-byte
    // payload slot} at ctx-table[16 * idx]) to its live AptValue*. pCtx = the const
    // chunk base (the string payloads are ctx-relative).
    AptValue* ResolveConstRecord(unsigned char* pRec, unsigned char* pCtx)
    {
        // xb1 0x14084AA67: `mov eax,[rcx]` -- the record type is a DWORD load.
        const int32_t nType = RecI32(pRec, 0x00);

        if (nType == 1)   // string: rebase payload against the ctx, INTERN, un-rebase
        {
            uintptr_t& rPayload = Slot(pRec, 0x08);
            if (rPayload)
                rPayload += reinterpret_cast<uintptr_t>(pCtx);
            // xb1 sub_140838AE0: the string-pool interner (chain-AddRef'd + GC-rooted),
            // NOT the temporary Create -- the interned ref is what the "Create's ref
            // transfers" slot convention below relies on.
            AptValue* pValue = AptStringPool_FindOrCreate(reinterpret_cast<const char*>(rPayload));
            if (rPayload)
                rPayload -= reinterpret_cast<uintptr_t>(pCtx);
            else
                rPayload = 0;
            return pValue;
        }

        // Payload reads are DWORD loads in the xb1 asm (vmovss / movsxd dword / cmp dword).
        switch (nType)
        {
            case 6:   // float: 0.0f collapses to the pooled integer 0 (xb1 vucomiss+jnz --
            {         // the UNORDERED result also falls through, so a NaN const takes the
                      // integer-0 path too; replicated with the explicit self-compare)
                const float fValue = RecF32(pRec, 0x08);
                if (fValue == 0.0f || fValue != fValue)
                    return AptInteger::Create(0);
                return AptFloat::Create(fValue);
            }
            case 7:   // integer (xb1: mov ecx,[rcx+8])
                return AptInteger::Create(RecI32(pRec, 0x08));
            case 8:   // lookup-pool entry (xb1: movsxd of the dword index; upper bound only)
            {
                const int nIndex = RecI32(pRec, 0x08);
                return AptLookup::PoolHolds(nIndex) ? AptLookup::GetPoolEntry(nIndex) : 0;
            }
            case 5:   // boolean: the shared singleton pair (xb1: cmp dword ptr [rcx+8],0)
                return AptBoolean::Create(RecI32(pRec, 0x08) != 0);
            case 4:   // register-file entry (xb1: movsxd of the dword index; upper bound only)
            {
                const int nIndex = RecI32(pRec, 0x08);
                return AptRegister::FileHolds(nIndex) ? AptRegister::GetFileEntry(nIndex) : 0;
            }
            case 3:
                return gpUndefinedValue;
            default:
                return 0;
        }
    }
}

// ---------------------------------------------------------------------------
// _parseStream (xb1 sub_14084A920) -- see the header comment. pnValueCount is the
// threaded resolved-value counter the callers accumulate (resolve: ++ per entry;
// unresolve: the sequential re-serialize index written back into each slot).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_parseStream(unsigned char* pStream, uintptr_t nBase,
                                        void* pConstCtx, int64_t* pnValueCount)
{
    const bool bUnresolve = (pConstCtx == 0);

    if (bUnresolve)
        gValuesToRelease.ReleaseValues();   // xb1: sub_14083ED90(&vector) on entry

    unsigned char* pc = pStream;
    unsigned int   nOp = *pc++;
    if (nOp == 0)
        return;

    while (true)
    {
        bool bDrain = !bUnresolve;   // LABEL_69 runs after every RESOLVE-direction op

        switch (nOp)
        {
            // ---- 4-byte inline payload, unaligned -------------------------------
            case 0x77:   // TraceStart
            case 0xB4:   // PushFloat
            case 0xB7:   // PushDWord
                pc += 4;
                bDrain = false;
                break;

            // ---- 4-byte inline payload, 8-aligned -------------------------------
            case 0x81:   // GotoFrame
            case 0x87:   // StoreRegister
            case 0x99:   // BranchAlways
            case 0x9D:   // BranchIfTrue
            case 0x9F:   // GotoFrame2
            case 0xB8:   // BranchIfFalse
                pc = Align8(pc) + 4;
                bDrain = false;
                break;

            // ---- one inline byte -------------------------------------------------
            case 0xA2: case 0xAE: case 0xAF: case 0xB0:
            case 0xB1: case 0xB2: case 0xB3: case 0xB5:
                pc += 1;
                bDrain = false;
                break;

            // ---- one inline big-endian word --------------------------------------
            case 0xA3: case 0xB6:
                pc += 2;
                bDrain = false;
                break;

            // ---- GetUrl: two 8-byte string pointers ------------------------------
            case 0x83:
            {
                unsigned char* pRec = Align8(pc);
                pc = pRec + 16;
                Rebase(Slot(pRec, 0x00), nBase, bUnresolve);   // url
                Rebase(Slot(pRec, 0x08), nBase, bUnresolve);   // target
                break;
            }

            // ---- one 8-byte string pointer ---------------------------------------
            case 0x8B:   // SetTarget
            case 0x8C:   // GotoLabel
            case 0xA1:   // (PushString: the direct string form)
            case 0xA4: case 0xA5: case 0xA6: case 0xA7:   // PushString{Get,Set}{Var,Member}
            {
                unsigned char* pRec = Align8(pc);
                pc = pRec + 8;
                Rebase(Slot(pRec, 0x00), nBase, bUnresolve);
                break;
            }

            // ---- With: PC-relative end-of-block ----------------------------------
            case 0x94:
            {
                unsigned char* pRec = Align8(pc);
                pc = pRec + 8;
                // The serialized slot is relative to the POST-OPERAND PC; resolve to
                // the absolute end pointer (xb1: *slot += pc / -= pc).
                if (bUnresolve)
                    Slot(pRec, 0x00) -= reinterpret_cast<uintptr_t>(pc);
                else
                    Slot(pRec, 0x00) += reinterpret_cast<uintptr_t>(pc);
                break;
            }

            // ---- Try: 24-byte record ---------------------------------------------
            case 0x8F:
            {
                unsigned char* pRec = Align8(pc);
                pc = pRec + 24;
                // flag bit2 (+0x0C) = catch-in-register: no name pointer to relocate.
                if ((RecU32(pRec, 0x0C) & 4u) == 0)
                    Rebase(Slot(pRec, 0x10), nBase, bUnresolve);   // catch-variable name
                break;
            }

            // ---- DefineFunction / DefineFunction2: 48-byte record ----------------
            case 0x8E:   // DefineFunction2 (16-byte {reg, name} arg records)
            case 0x9B:   // DefineFunction  (8-byte flat name pointers)
            {
                unsigned char* pRec = Align8(pc);
                pc = pRec + 48;

                Rebase(Slot(pRec, 0x00), nBase, bUnresolve);       // function name
                if (!bUnresolve)
                    Rebase(Slot(pRec, 0x10), nBase, false);        // arg table (resolve first)

                // xb1 0x14084AE52/0x14084AD78: `cmp [r8+8], r9d` -- nArgs is a signed
                // DWORD (the u16 register-count/preload-flags pack occupies +0x0C/+0x0E,
                // so a qword read would drag them into the count).
                const int32_t nArgs = RecI32(pRec, 0x08);
                unsigned char* pArgs = reinterpret_cast<unsigned char*>(Slot(pRec, 0x10));
                if (nArgs > 0 && pArgs)
                {
                    const int nStride  = (nOp == 0x8E) ? 16 : 8;
                    const int nNameOff = (nOp == 0x8E) ? 8  : 0;
                    for (int32_t i = 0; i < nArgs; ++i)
                        Rebase(Slot(pArgs + i * nStride, nNameOff), nBase, bUnresolve);
                }

                if (bUnresolve)
                {
                    Rebase(Slot(pRec, 0x10), nBase, true);         // arg table (unresolve last)
                    // xb1 poison stamps over the (now-dead) runtime slots.
                    // xb1 0x14084ADCD: `mov dword ptr [r8+20h], 98765432h` -- a DWORD stamp
                    // (only +0x20..0x23; +0x24..0x27 stays untouched).
                    *reinterpret_cast<uint32_t*>(pRec + 0x20) = 0x98765432u;   // serialized .apt record poison stamp
                    Slot64(pRec, 0x28) = 0x12345678;
                }
                break;
            }

            // ---- Push / DefineDictionary: the constant-entry block ---------------
            case 0x88:   // DefineDictionary (the movie's string/constant dictionary)
            case 0x96:   // Push (the multi-value operand block)
            {
                unsigned char* pRec = Align8(pc);
                pc = pRec + 16;

                // xb1 0x14084AA3C: `cmp [rsi], edi` -- the entry count is a signed DWORD.
                const int32_t nCount = RecI32(pRec, 0x00);

                if (bUnresolve)
                {
                    // Release each live entry (a defined string returns to the pool --
                    // a boxed one through its internal AptString at +0x40), then write
                    // the sequential re-serialize index; finally un-rebase the table.
                    unsigned char* pTable = reinterpret_cast<unsigned char*>(Slot(pRec, 0x08));
                    for (int32_t i = 0; i < nCount; ++i)
                    {
                        AptValue* pValue = *reinterpret_cast<AptValue**>(pTable + i * 8);
                        if (pValue->isString())
                        {
                            AptValue* pRaw = pValue;
                            if (pRaw->getVtblIndex() != AptVFT_StringValue)
                                pRaw = *reinterpret_cast<AptValue**>(
                                    reinterpret_cast<char*>(pRaw) + 0x40);   // the boxed slot
                            AptStringPoolReleaseString(static_cast<AptString*>(pRaw));
                        }
                        else
                        {
                            pValue->Release();
                        }
                        Slot64(pTable + i * 8, 0) = *pnValueCount;
                        ++*pnValueCount;
                    }
                    Rebase(Slot(pRec, 0x08), nBase, true);
                    break;
                }

                // RESOLVE: rebase the table, then turn each serialized constant index
                // into its live AptValue* (through the ctx's 16-byte record table).
                Rebase(Slot(pRec, 0x08), nBase, false);
                unsigned char* pTable = reinterpret_cast<unsigned char*>(Slot(pRec, 0x08));
                unsigned char* pCtx   = static_cast<unsigned char*>(pConstCtx);
                unsigned char* pConstTable =
                    reinterpret_cast<unsigned char*>(Slot(pCtx, 0x28));   // [xb1: ctx+40] serialized .apt const-chunk record table (blob idiom)

                for (int32_t i = 0; i < nCount; ++i)
                {
                    int64_t nConstIndex = Slot64(pTable + i * 8, 0);
                    ++*pnValueCount;

                    // GUIAPT64 packing tolerance (deliberate; same family as the
                    // frame-table bug): a misaligned emitter record stores the
                    // serialized index in the HIGH dword of the 8-byte slot (observed
                    // 0x1fc00000000 == index 0x1FC << 32 in MAIN.bundle). Decode it;
                    // the offline apt8_fix_frametables repair owns the permanent data fix.
                    if (nConstIndex > 0xFFFF && (nConstIndex & 0xFFFFFFFFll) == 0
                        && (nConstIndex >> 32) <= 0xFFFF)
                    {
                        nConstIndex >>= 32;
                    }

                    // x64 wild-slot hardening (deliberate): a slot holding neither a
                    // sane index form is a live pointer from a prior resolve pass or
                    // garbage -- `pConstTable + 16 * <pointer>` AV'd MAIN's Fixup
                    // (2026-07-05, cdb-verified). Keep it as-is rather than crash.
                    if (nConstIndex < 0 || nConstIndex > 0xFFFF)
                        continue;

                    AptValue* pValue =
                        ResolveConstRecord(pConstTable + 16 * nConstIndex, pCtx);

                    *reinterpret_cast<AptValue**>(pTable + i * 8) = pValue;
                    // AddRef -- unless a defined string (Create's ref transferred).
                    if (pValue && !pValue->isString())
                        pValue->AddRef();

                    if ((i % 16) == 0)
                        gValuesToRelease.ReleaseValues();   // the periodic drain
                }
                break;
            }

            default:
                bDrain = false;
                break;
        }

        if (bDrain)
            gValuesToRelease.ReleaseValues();   // LABEL_69: the per-op resolve drain

        nOp = *pc++;
        if (nOp == 0)
            return;
    }
}
