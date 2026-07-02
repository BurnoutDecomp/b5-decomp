#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptScriptFunction2.
//
// The value produced by the AS2 `DefineFunction2` action: a user-defined script
// function with a declared register layout and the SWF "preload" flags that
// pre-bind this / super / arguments / _root / _parent / _global into call
// registers on entry. It is the AptScriptFunctionBase subclass the interpreter
// hands back from AptActionInterpreter::_FunctionAptActionDefineFunction2,
// holding a pointer to the compiled-function record parsed out of the movie's
// action stream.
//
// SHAPE recovered from the X360 ARTIST.XEX (no Feb-2007 / DecFIGS header in scope):
//     AptScriptFunction2::AptScriptFunction2   @ 0x82AF13F8
//     AptScriptFunction2::operator new         @ 0x82AE61C8
//     AptScriptFunction2::operator delete      @ 0x82AF14B0
//     AptScriptFunction2::GetByteCodeBase      @ 0x82AF1470
//     AptScriptFunction2::GetConstantPool      @ 0x82AF1490
//     AptScriptFunction2::SetArgument          @ 0x82AF53A0
//     AptScriptFunction2::SetupBeforeExecution @ 0x82B02558
//     AptScriptFunction2::CleanupAfterExecution@ 0x82AD6708
//     AptScriptFunction2::`scalar deleting destructor' @ 0x82AF5BC8
//
// BASE: the ctor forwards to AptScriptFunctionBase(eType = AptVFT_ScriptFunction2
// = 35, ...) -- confirmed by `li r4, 0x23` (35) to the base ctor -- then installs
// its own vtable (off_82145D10) and stores the compiled-function record pointer.
//
// LAYOUT (sizeof = 52 / 0x34, pinned by `operator new(52)` and `operator delete
// (this, 52)`):
//   AptScriptFunctionBase base ....... 48 bytes (ends at +0x30)
//   mpByteCode  AptScriptFunction2ByteCode* ... +0x30   the compiled-function record
//
// vtable object-type index = AptVFT_ScriptFunction2 (35). It is a garbage-collected
// value (AptValueGC base via AptObject), so -- like AptScriptFunction1 /
// AptNativeFunction and unlike the non-GC leaves -- operator new/delete route
// through gpGCPoolManager and flip the AptValueGC_MemItem "allocated" flag (bodies
// in the .cpp, which keeps the pool / MemItem headers out of this header).
//
// This is vendor/SDK code reconstructed in its canonical home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptScriptFunction2, the
// AptVFT_* index, GetByteCodeBase / GetConstantPool / SetArgument, ...) are an
// external/middleware API and kept verbatim.
// ===========================================================================

#include <cstddef>   // size_t
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"   // base + AptConstantPool + AptVFT_*

class AptValue;

// ---------------------------------------------------------------------------
// One entry of the DefineFunction2 register-parameter table: each declared
// parameter is either pre-bound into a numbered call register (mnRegister != 0)
// or passed by name (mnRegister == 0 -> bound into the frame's locals under
// mpName). Serialised in the movie data; 8 bytes per entry.
// ---------------------------------------------------------------------------
struct AptScriptFunction2Arg      // the GUIAPT64 16-byte arg record (8-aligned)
{
    int32_t     mnRegister;   // +0x00  target register (0 -> bind by name instead)
    int32_t     mPad04;       // +0x04  (alignment)
    const char* mpName;       // +0x08  the parameter name (qword movie-string pointer)
};

// ---------------------------------------------------------------------------
// The compiled DefineFunction2 (v2) record mpByteCode points at. SERIALISED data
// parsed out of the movie's ActionScript stream (its layout is fixed by the
// .apt/SWF DefineFunction2 tag, not by a C++ class), so its fields are a fixed,
// known layout -- modelled as named members (NOT raw offset pokes). The argument
// table, constant pool and name are pointers into the same movie buffer; the
// bytecode bytes follow the header inline (GetByteCodeBase returns &maByteCode[0]).
//
// This differs from the DefineFunction (v1) AptScriptFunction1ByteCode: v2 carries
// the preload-flags word + the register-parameter table pointer (mpArgTable) and a
// separate body length, where v1 has a plain argument-name array and the bytecode
// size inline. The constant-pool fields (mppConstantPool / mnConstantPoolCount) are
// patched in place by the interpreter when the DefineFunction2 action runs (from
// the interpreter's two constant-pool registers).
// ---------------------------------------------------------------------------
// The GUIAPT64 48-byte serialized record (8-aligned; verified against the libapt2
// converter's DefineFunction2 writer + the XB1 _parseStream walk): name qword,
// {u32 params, u16 registers, u16 preload-flags}, arg-table qword, u32 body length
// (+pad), then the two signature qwords (0x98765432/0x12345678) the runtime handler
// overwrites with the live constant-pool registers.
struct AptScriptFunction2ByteCode
{
    const char*           mpName;              // +0x00  the function's name ("" for anonymous)
    uint32_t              mnNumArguments;      // +0x08  declared parameter count
    uint16_t              mnRegisters;         // +0x0C  declared register count
    uint16_t              muPreloadFlags;      // +0x0E  preload-register flags (KU_PRELOAD_*)
    AptScriptFunction2Arg* mpArgTable;         // +0x10  array[mnNumArguments] of register/name entries
    uint32_t              mnBodyLength;        // +0x18  record-body length (u32; libapt2 Write<u32>)
    uint32_t              mPad1C;              // +0x1C  writer align-8 pad (zero-filled)
    const char**          mppConstantPool;     // +0x20  sig1 slot -> the constant-pool table (patched in)
    int64_t               mnConstantPoolCount; // +0x28  sig2 slot -> number of entries (patched in)
    uint8_t               maByteCode[1];       // +0x30  the action bytecode (flexible)
};

// DefineFunction2 register-preload flag bits (SetupBeforeExecution masks against
// muPreloadFlags). Named for the register each requests be pre-bound on entry.
enum AptScriptFunction2PreloadFlags
{
    KU_PRELOAD_THIS      = 0x0001,   // bind the next register <- the function's "this"
    KU_PRELOAD_SUPER     = 0x0004,   // bind the next register <- super (undefined in this build)
    KU_PRELOAD_ARGUMENTS = 0x0010,   // bind the next register <- the "arguments" object
    KU_PRELOAD_ROOT      = 0x0040,   // bind the next register <- _root
    KU_PRELOAD_PARENT    = 0x0080,   // bind the next register <- _parent
    KU_PRELOAD_GLOBAL    = 0x0100    // bind the next register <- _global
};

class AptScriptFunction2 : public AptScriptFunctionBase
{
public:
    // ---- GC pool new / delete (X360 operator new @0x82AE61C8 / delete @0x82AF14B0)
    // A script function is a garbage-collected value, so -- like AptScriptFunction1 /
    // AptNativeFunction and unlike the non-GC leaves -- it allocates from the GC pool
    // (gpGCPoolManager) and flips the AptValueGC_MemItem "allocated" flag. Defined
    // out-of-line in AptScriptFunction2.cpp.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // Construct a DefineFunction2 value.   @ 0x82AF13F8
    //   pCallContext : the interpreter's current CIH value -- when non-null the base
    //                  ctor snapshots the live frame stack as this closure's scope.
    //   pByteCode    : the parsed compiled-function record (stored at +0x30).
    //   pCIH         : the character-instance-handle value to bind to (base +0x20).
    AptScriptFunction2(AptValue* pCallContext,
                       AptScriptFunction2ByteCode* pByteCode,
                       AptValue* pCIH);

    // ---- AptScriptFunctionBase overrides --------------------------------------
    // GetByteCodeBase @0x82AF1470 -- the action bytecode body, which follows the
    // compiled-record header inline (+0x1C == &maByteCode[0]).
    virtual void* GetByteCodeBase() const;
    // GetConstantPool @0x82AF1490 -- the constant-pool string table + its entry count.
    virtual AptConstantPool GetConstantPool() const;
    // SetArgument @0x82AF53A0 -- bind call argument nArgIndex to pValue: into the
    // pre-declared register slot when the record declares one for this index,
    // otherwise into the current frame stack's locals under the declared name.
    virtual void SetArgument(int32_t nArgIndex, AptValue* pValue);

    // SetupBeforeExecution @0x82B02558 -- snapshot + clear the current frame stack /
    // register window into pSaved, then pre-bind the registers the record's preload
    // flags request (this / super / arguments / _root / _parent / _global). (Default
    // arguments are declared once on the base virtual, not repeated here.)
    virtual void SetupBeforeExecution(SavedExecutionState* pSaved,
                                      AptValue* pArgScope,
                                      AptValue* pPreloadThis,
                                      AptValue* pPreloadArgs);
    // CleanupAfterExecution @0x82AD6708 -- release every register bound this call,
    // then restore the saved register window + frame stack (via the base) from pSaved.
    virtual void CleanupAfterExecution(SavedExecutionState* pSaved);

protected:
    // ~AptScriptFunction2 has no work of its own (the compiled record is owned by the
    // movie data, not the value); the X360 emits only the standard deleting-destructor
    // thunk (restore the base vtable -> ~AptObject -> conditional operator delete),
    // which is compiler-generated and intentionally not hand-written.
    virtual ~AptScriptFunction2() {}

private:
    // +0x30 -- the compiled DefineFunction2 record (owned by the movie data).
    AptScriptFunction2ByteCode* mpByteCode;

    AptScriptFunction2();   // not defined: a function always needs its record
};
