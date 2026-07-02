#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptScriptFunctionByteCodeBlock.
//
// The script-function value that carries its compiled body INLINE: unlike
// AptScriptFunction1 / AptScriptFunction2 (which hold a pointer to a shared
// compiled-function record and read the byte-code / constant pool out of it),
// AptScriptFunctionByteCodeBlock stores the byte-code base and constant-pool
// descriptor directly in its own trailing members. It is the "byte-code block
// payload" the X360 builds for an event-handler clip script (constructed by
// AptCIH::queueClipEvents).
//
// SHAPE: no Feb-2007 / DecFIGS header is in scope for this class (X360-only spine
// entity), so the LAYOUT + bodies are recovered from the X360 ARTIST.XEX:
//     AptScriptFunctionByteCodeBlock::AptScriptFunctionByteCodeBlock @ 0x82AF15C0
//     AptScriptFunctionByteCodeBlock::GetByteCodeBase                @ 0x82AD4FB0
//     AptScriptFunctionByteCodeBlock::GetConstantPool                @ 0x82AF1620
//     AptScriptFunctionByteCodeBlock::operator new                   @ 0x82AE6218
//     AptScriptFunctionByteCodeBlock::operator delete                @ 0x82AF1638
//     AptScriptFunctionByteCodeBlock::`vector deleting destructor'   (compiler thunk -- dropped)
//
// BASE: the ctor forwards to AptScriptFunctionBase::AptScriptFunctionBase(eType=
// AptVFT_ScriptFunctionByteCodeBlock (36), pCallContext, pCIH, bMakePrototype=
// false) and installs its own vtable (off_82145D78), so it IS an
// AptScriptFunctionBase (a scriptable AS function value) with its byte-code +
// constant-pool members appended.
//
// LAYOUT (console offsets, pinned by the ctor stores @0x82AF15C0 + GetByteCodeBase
// / GetConstantPool; x64 widens the vtable pointer so absolute offsets shift --
// members are recovered by NAME, semantic parity not byte offsets):
//   AptScriptFunctionBase base ....... +0x00 .. +0x2F
//   mpByteCode      void* ............ +0x30  byte-code stream base
//                                             (GetByteCodeBase returns it directly,
//                                             c.f. AptScriptFunction1 which returns
//                                             record+24)
//   mnByteCodeSize  int32_t .......... +0x34  byte-code extent (ctor a3; FLAG: the
//                                             companion size to mpByteCode -- no
//                                             accessor in this TU exposes it, so the
//                                             name is the natural inference)
//   mnArgumentInfo  int32_t .......... +0x38  ctor a5 (FLAG: argument/flags word;
//                                             role not attested by any accessor in
//                                             this TU -- named conservatively)
//   mConstantPool   AptConstantPool .. +0x3C  the constant-pool descriptor
//                                             {entries, count}, stored as one 8-byte
//                                             value (ctor `std`); GetConstantPool
//                                             returns it by value.
//
// vtable object-type index = AptVFT_ScriptFunctionByteCodeBlock (36), confirmed by
// the ctor's `li r4, 0x24` (36) argument to the base.
//
// This is vendor/SDK code reconstructed in its canonical home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptScriptFunctionByteCodeBlock,
// the AptVFT_* index, GetByteCodeBase / GetConstantPool, AptConstantPool) are an
// external/middleware API and kept verbatim.
// ===========================================================================

#include <cstddef>   // size_t
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"   // AptScriptFunctionBase base + AptConstantPool + AptVFT_*

class AptScriptFunctionByteCodeBlock : public AptScriptFunctionBase
{
public:
    // ---- GC pool new / delete (X360 operator new @0x82AE6218 / delete @0x82AF1638)
    // AptScriptFunctionByteCodeBlock is a garbage-collected AS value, so its block
    // comes from the GC value pool (gpGCPoolManager) and operator new flips the
    // AptValueGC_MemItem "allocated" flag -- exactly the GC value family pattern
    // (AptNativeFunction / AptPrototype / AptArray). Defined out-of-line in the .cpp.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // ctor @ 0x82AF15C0 -- chain to AptScriptFunctionBase(36, pCallContext, pCIH,
    // /*bMakePrototype*/ false), then store the inline compiled body (byte-code base
    // + size, the argument/flags word, and the constant-pool descriptor) and install
    // the AptScriptFunctionByteCodeBlock vtable.
    //   pByteCode     : the byte-code stream base (a2).
    //   nByteCodeSize : byte-code extent (a3; see FLAG in the layout note).
    //   constantPool  : the {entries, count} constant-pool descriptor (a4, passed by
    //                   value -- one 8-byte register on the console).
    //   pFunctionName : the handler-name chars (a5 -- ATTESTED 2026-07-01 by the
    //                   PS3 queueClipEvents call site @0x815BD0, which passes
    //                   saConstant[code]'s data chars; was FLAG "argument/flags").
    //   pCIH          : the creating character-instance-handle value (a6 -> base "CIH").
    //   pCallContext  : the enclosing call/scope context (a7 -> base pCallContext).
    AptScriptFunctionByteCodeBlock(void* pByteCode,
                                   int32_t nByteCodeSize,
                                   AptConstantPool constantPool,
                                   const char* pFunctionName,
                                   AptValue* pCIH,
                                   AptValue* pCallContext);

    // ---- AptScriptFunctionBase overrides ---------------------------------------
    // GetByteCodeBase @0x82AD4FB0 -- the inline byte-code base (returned directly).
    virtual void*           GetByteCodeBase() const;   // @0x82AD4FB0
    // GetConstantPool @0x82AF1620 -- the inline constant-pool descriptor, by value.
    virtual AptConstantPool GetConstantPool() const;   // @0x82AF1620

protected:
    // ~AptScriptFunctionByteCodeBlock has no work of its own: the base
    // AptScriptFunctionBase teardown (and the embedded AptNativeHash) handles the
    // members; the byte-code / constant-pool members are POD. The X360 emits only
    // the standard `vector deleting destructor' thunk (dropped, not hand-written).
    virtual ~AptScriptFunctionByteCodeBlock() {}

private:
    // +0x30 -- the inline byte-code stream base (ctor a2).
    void*           mpByteCode;
    // +0x34 -- the byte-code extent (ctor a3; FLAG: inferred companion size).
    int32_t         mnByteCodeSize;
    // +0x38 -- the handler-name chars (ctor a5; console 4-byte slot, pointer on
    // x64 -- attested as the clip-event handler name by the queueClipEvents site).
    const char*     mpFunctionName;
    // +0x3C -- the constant-pool descriptor (ctor a4, an 8-byte by-value store);
    // GetConstantPool returns it.
    AptConstantPool mConstantPool;

    AptScriptFunctionByteCodeBlock();   // not defined: a compiled body is always required
};
