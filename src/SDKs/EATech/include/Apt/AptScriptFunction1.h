#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptScriptFunction1.
//
// The value produced by the AS2 `DefineFunction` action: a user-defined script
// function (the v1 / non-"function2" form). It is the AptScriptFunctionBase
// subclass the interpreter hands back from
// AptActionInterpreter::_FunctionAptActionDefineFunction, holding a pointer to the
// compiled-function record parsed out of the movie's action stream.
//
// SHAPE recovered from the X360 ARTIST.XEX (no Feb-2007 / DecFIGS header in scope):
//     AptScriptFunction1::AptScriptFunction1   @ 0x82AF1308
//     AptScriptFunction1::operator new         @ 0x82AE6178
//     AptScriptFunction1::operator delete      @ 0x82AF13A0
//     AptScriptFunction1::GetName              @ 0x82AF1450
//     AptScriptFunction1::GetNumArguments      @ 0x82AF1460
//     AptScriptFunction1::GetByteCodeBase      @ 0x82AF1360
//     AptScriptFunction1::GetByteCodeSize      @ 0x82AF1370
//     AptScriptFunction1::GetConstantPool      @ 0x82AF1380
//     AptScriptFunction1::SetArgument          @ 0x82AF5338
//     AptScriptFunction1::Duplicate            @ 0x82B024A8
//        (copy ctor @0x82B01000, base copy ctor @0x82B00E90)
//
// BASE: the ctor forwards to AptScriptFunctionBase(eType = AptVFT_ScriptFunction1
// = 34, ...) -- confirmed by `li r4, 0x22` (34) to the base ctor -- then installs
// its own vtable (off_82145CA8) and stores the compiled-function record pointer.
//
// LAYOUT (sizeof = 52 / 0x34, pinned by `operator new(52)` and `operator delete
// (this, 52)`):
//   AptScriptFunctionBase base ....... 48 bytes (ends at +0x30)
//   mpByteCode  AptScriptFunction1ByteCode* ... +0x30   the compiled-function record
//
// vtable object-type index = AptVFT_ScriptFunction1 (34).
//
// This is vendor/SDK code reconstructed in its canonical home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptScriptFunction1, the
// AptVFT_* index, GetByteCodeBase / GetConstantPool / SetArgument, ...) are an
// external/middleware API and kept verbatim.
// ===========================================================================

#include <cstddef>   // size_t
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"   // base + AptConstantPool + AptVFT_*

struct AptValue;

// ---------------------------------------------------------------------------
// The compiled DefineFunction (v1) record mpByteCode points at. This is a
// SERIALISED data structure parsed out of the movie's ActionScript stream (its
// layout is fixed by the .apt/SWF DefineFunction tag, not by a C++ class), so its
// fields are accessed positionally -- but they are a fixed, known layout, so they
// are modelled as named members (NOT raw offset pokes). The argument names and the
// constant pool are pointers into the same movie buffer; the bytecode bytes follow
// the header inline (GetByteCodeBase returns &maByteCode[0]).
//
// NOTE: AptScriptFunction2 (DefineFunction2) uses a DIFFERENT record layout (one
// extra dword before the constant pool / bytecode), so it has its own record type;
// this struct is the v1 form only.
// ---------------------------------------------------------------------------
struct AptScriptFunction1ByteCode
{
    const char*  mpName;               // +0x00  the function's name (movie string; "" for anonymous)
    int32_t      mnNumArguments;       // +0x04  declared parameter count
    const char** mppArgumentNames;     // +0x08  array[mnNumArguments] of parameter-name strings
    int32_t      mnByteCodeSize;       // +0x0C  length in bytes of the action bytecode
    const char** mppConstantPool;      // +0x10  the constant-pool string table
    int32_t      mnConstantPoolCount;  // +0x14  number of constant-pool entries
    uint8_t      maByteCode[1];        // +0x18  the action bytecode (flexible; mnByteCodeSize bytes)
};

class AptScriptFunction1 : public AptScriptFunctionBase
{
public:
    // ---- GC pool new / delete (X360 operator new @0x82AE6178 / delete @0x82AF13A0)
    // A script function is a garbage-collected value, so -- like AptArray /
    // AptNativeFunction and unlike the non-GC leaves -- it allocates from the GC
    // pool (gpGCPoolManager) and flips the AptValueGC_MemItem "allocated" flag.
    // Defined out-of-line in AptScriptFunction1.cpp.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // Construct a DefineFunction value.   @ 0x82AF1308
    //   pCallContext : the interpreter's current CIH value -- when non-null the base
    //                  ctor snapshots the live frame stack as this closure's scope.
    //   pByteCode    : the parsed compiled-function record (stored at +0x30).
    //   pCIH         : the character-instance-handle value to bind to (base +0x20).
    AptScriptFunction1(AptValue* pCallContext,
                       AptScriptFunction1ByteCode* pByteCode,
                       AptValue* pCIH);

    // ---- AptScriptFunctionBase overrides --------------------------------------
    virtual const char*     GetName() const;          // @0x82AF1450
    virtual int32_t         GetNumArguments() const;  // @0x82AF1460
    virtual void*           GetByteCodeBase() const;  // @0x82AF1360
    virtual int32_t         GetByteCodeSize() const;  // @0x82AF1370
    virtual AptConstantPool GetConstantPool() const;  // @0x82AF1380
    virtual void            SetArgument(int32_t nArgIndex, AptValue* pValue);  // @0x82AF5338

    // Deep-copy this function value, re-binding the copy to pCIH.   @ 0x82B024A8
    virtual AptScriptFunction1* Duplicate(AptValue* pCIH) const;

protected:
    // ~AptScriptFunction1 has no work of its own (the compiled record is owned by
    // the movie, not the value); the X360 emits only the standard deleting-destructor
    // thunk (restore the base vtable -> ~AptObject -> conditional operator delete),
    // which is compiler-generated and intentionally not hand-written.
    virtual ~AptScriptFunction1() {}

private:
    // +0x30 -- the compiled DefineFunction record (owned by the movie data).
    AptScriptFunction1ByteCode* mpByteCode;

    // copy ctor @0x82B01000 -- forward to the base copy ctor (re-binding to pCIH)
    // and copy the compiled-record pointer. Used only by Duplicate.
    AptScriptFunction1(const AptScriptFunction1& rOther, AptValue* pCIH);

    AptScriptFunction1();   // not defined: a function always needs its record
};
