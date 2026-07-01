#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptScriptFunctionBase.
//
// The shared base of the three ActionScript user-function value types:
//     AptScriptFunction1               (DefineFunction,  AptVFT_ScriptFunction1 = 34)
//     AptScriptFunction2               (DefineFunction2, AptVFT_ScriptFunction2 = 35)
//     AptScriptFunctionByteCodeBlock   (AptVFT_ScriptFunctionByteCodeBlock = 36)
//
// A script function is an AS *object* (it has named members + a prototype) that
// also carries the lexical/scope state needed to execute its compiled body: the
// character-instance handle it is bound to (mpCIH), the timeline animation it was
// defined on (mpParentAnim), and a snapshot of the enclosing call frame
// (mpParentScope) so closures resolve free variables. The compiled body itself
// (name / argument names / bytecode / constant pool) is supplied by the concrete
// subclass; this base owns only the scope machinery and the static frame-stack /
// register-array execution state.
//
// SHAPE recovered from the X360 ARTIST.XEX (no Feb-2007 / DecFIGS header is in
// scope for this class):
//     AptScriptFunctionBase::AptScriptFunctionBase   @ 0x82AF1030
//     AptScriptFunctionBase::CreateFrameStack        @ 0x82AF1260
//     AptScriptFunctionBase::RegisterReferences      @ 0x82AE27B0
//     AptScriptFunctionBase::DestroyGCPointers       @ 0x82AF1508
//     AptScriptFunctionBase::SetRegisterValue        @ 0x82AD6690
//     AptScriptFunctionBase::`scalar deleting destructor' @ 0x82AF5B08
//     ...(SetupBeforeExecution / CleanupAfterExecution / GetInScopeChain /
//        SetInLocalScope / ExistsInLocalScope / SetWhereExistsInScopeChain /
//        CreatingNestedFunction / InitializeStaticData / ShutdownStaticData are
//        the base's own TU; declared here so the subclasses + the interpreter can
//        call them by name -- their bodies land with class:AptScriptFunctionBase.)
//
// BASE: the ctor forwards to AptValueWithHash::AptValueWithHash(eType, 8) and then
// clears the AptObject class-flags word at +0x1C (rlwinm r10,r10,0,10,7 -- the same
// hasClass bitfield family AptObject owns); the `scalar deleting destructor' tears
// down through AptObject::~AptObject. So this derives from AptObject (a scriptable
// value), and the ctor's direct AptValueWithHash call is AptObject's trivial ctor
// inlined by the X360 compiler.
//
// LAYOUT (sizeof = 48 / 0x30, pinned by `operator delete(this, 48)` in the thunk):
//   AptObject base .................... 32 bytes (vtable + bitfield + AptNativeHash
//                                       + mClassFlags), ends at +0x20
//   mpCIH         AptValue* .......... +0x20  the bound character-instance handle
//                                              value ("CIH" in RegisterReferences)
//   mpParentAnim  AptValue* .......... +0x24  the timeline animation it was defined
//                                              on ("ParentAnim")
//   mpParentScope AptValue* .......... +0x28  the captured enclosing call frame
//                                              (an AptFrameStack value, "ParentScope")
//   mnCreatingNestedFunction uint16_t  +0x2C  set while a nested DefineFunction is
//                                              being built (read by CreateFrameStack)
//
// vtable object-type index is supplied by the subclass (li r4, 34/35/36 to the ctor).
//
// The execution state (the current frame stack + the flat register array) is
// PROCESS-WIDE, not per-instance: the X360 keeps it in .data globals
// (off_8324E3DC / off_8324E3D0 / dword_8324E3D4), modelled here as the leak's
// static members.
//
// This is vendor/SDK code reconstructed in its canonical home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptScriptFunctionBase, the
// AptVFT_* index, GetRegisterValue, ...) are an external/middleware API and kept
// verbatim.
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptObject.h"             // AptObject base + AptVFT_*
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC keys

class AptValue;
struct AptFrameStack;   // SDKs/EATech/include/Apt/AptFrameStack.h (the captured-scope value)

// The small {entries, count} pair AptScriptFunction*::GetConstantPool returns from
// the compiled-function record (the X360 returns it via a hidden two-dword result
// pointer; modelled here as a by-value POD).
struct AptConstantPool
{
    const char** mppEntries;   // the constant-pool string table
    int32_t      mnCount;      // number of entries
};

struct AptScriptFunctionBase : public AptObject
{
public:
    // The per-call execution state SetupBeforeExecution snapshots and
    // CleanupAfterExecution restores: the frame stack + flat register window that
    // were current before this function was entered. The interpreter stack-allocates
    // one of these per call (the X360 passes its address to both methods).
    struct SavedExecutionState
    {
        AptFrameStack* mpSavedFrameStack;   // +0x00  previous spFrameStack
        AptValue**     mpSavedRegisters;    // +0x04  previous spRegisters base
    };

    // ---- shared overridable script-function interface --------------------------
    // The three concrete function types each install their own vtable and override
    // these; the interpreter calls them through the AptScriptFunctionBase* it gets
    // back from a DefineFunction handler. Declared virtual here (the subclass
    // vtables differ only in these slots + the GC/teardown slots); the pseudocode
    // renders the calls direct, but three distinct overriding vtables prove they are
    // virtual. Bodies live in each subclass TU.
    virtual const char* GetName() const;
    virtual int32_t     GetNumArguments() const;
    virtual void*       GetByteCodeBase() const;
    virtual int32_t     GetByteCodeSize() const;
    virtual AptConstantPool GetConstantPool() const;
    virtual void        SetArgument(int32_t nArgIndex, AptValue* pValue);

    // Deep-copy this function value, re-binding it to pCIH (used when a closure is
    // captured into a new character context). Returns null if the pool allocation
    // fails.
    virtual AptScriptFunctionBase* Duplicate(AptValue* pCIH) const;

    // ---- per-call execution scaffolding (bodies in class:AptScriptFunctionBase) -
    // SetupBeforeExecution: snapshot the current frame stack + register window into
    // pSaved, install a fresh window, and pre-bind the registers this function's
    // declaration requests (the DefineFunction2 preload flags resolve this / super /
    // arguments / _root / _parent / _global). pArgScope is the path scope used to
    // resolve "arguments"; pPreloadThis / pPreloadArgs are pre-resolved overrides
    // (null -> resolve via findChild). The base version (@0x82AD6618) only does the
    // save/clear; AptScriptFunction2 (@0x82B02558) overrides with the full preload.
    // (The trailing args default so a plain save/clear caller -- e.g. the byte-code
    // block -- can invoke it with just the saved-state out parameter.)
    virtual void SetupBeforeExecution(SavedExecutionState* pSaved,
                                      AptValue* pArgScope = 0,
                                      AptValue* pPreloadThis = 0,
                                      AptValue* pPreloadArgs = 0);   // @0x82AD6618
    // CleanupAfterExecution: restore the frame stack (base @0x82AD6630) -- and, in
    // the AptScriptFunction2 override (@0x82AD6708), release this call's registers
    // and restore the saved register window -- from pSaved.
    virtual void CleanupAfterExecution(SavedExecutionState* pSaved);  // @0x82AD6630

    // ---- GC virtual overrides (this TU's base owns the scope-pointer marks) -----
    virtual void RegisterReferences();        // @0x82AE27B0
    virtual void DestroyGCPointers();         // @0x82AF1508

    // ---- scope / local-variable machinery (class:AptScriptFunctionBase) ---------
    // The current activation's local-variable frame: create it lazily (CreateFrameStack)
    // and query / mutate the lexical scope chain. SetArgument (in the subclasses)
    // calls CreateFrameStack when no frame is live yet.
    void      CreateFrameStack();             // @0x82AF1260
    bool      ExistsInLocalScope(const EAStringC& key);          // @0x82AE1938
    AptValue* GetInScopeChain(const EAStringC& key);            // @0x82AE1990
    void      SetInLocalScope(const EAStringC& key, AptValue* pValue);  // @0x82AF52C0
    bool      SetWhereExistsInScopeChain(const EAStringC& key, AptValue* pValue);  // @0x82AF5308
    bool      CreatingNestedFunction() const; // @0x82AF12F0

    // Store pValue into flat register slot nRegister (the AS "register" file the
    // bytecode addresses), growing the live-register high-water mark. @0x82AD6690
    static void SetRegisterValue(int32_t nRegister, AptValue* pValue);

    // Read flat register slot nRegister (the symmetric read side of SetRegisterValue;
    // the tiny indexed accessor the console inlines -- stackPushIndirect resolves an
    // AptVFT_Register value through it). No bounds check (matches the console).
    static AptValue* GetRegisterValue(int32_t nRegister);

    // ---- static execution-state lifetime ---------------------------------------
    // Allocate (nRegisterCount) the flat register window from the operand-stack pool
    // (the console reads the count from the AptInitParmsT it is passed).
    static void InitializeStaticData(int32_t nRegisterCount);   // @0x82AE26C0
    static void ShutdownStaticData();         // @0x82AE2758

    // PopStaticData (PS3 @0x7E0AB8) -- pop the current register-block frame back to
    // pSavedBase (the saved arg-heap pointer): Release each slot in the current frame +
    // reset it to undefined, then move the base to pSavedBase and set the count to span
    // [pSavedBase, oldBase). Verified from the PS3 asm: the base is the console method's
    // `this` (the void* arg is unused); the per-slot call is Release (vtable slot 1).
    static void PopStaticData(void* pSavedBase);

    // PushStaticData (PS3 @0x7E0A90) -- start a new (empty) register-block frame: return
    // the current base as the restore point, advance the base past the current frame
    // (base += count), and zero the count. The caller saves the returned base and later
    // hands it to PopStaticData to pop back. (The inverse of PopStaticData.)
    static AptValue** PushStaticData();

    AptValue* GetCIH() const          { return mpCIH; }
    AptValue* GetParentScope() const  { return mpParentScope; }
    // +0x24 ParentAnim accessor (the global-object frame the interpreter's variable
    // fallback resolves _level / global members through -- getVariable @0x82B03430
    // reads it as *(mpCurrentFunction+36)).
    AptValue* GetParentAnim() const   { return mpParentAnim; }

    // The current live local-variable frame stack (off_8324E3DC); the interpreter's
    // scope-chain walkers consult it before falling back to mpParentScope.
    static AptFrameStack* GetActiveFrameStack() { return spFrameStack; }

protected:
    // ctor @0x82AF1030 -- chain to AptObject(eType, 8), bind to pCIH, derive the
    // owning animation, and (when pCallContext is non-null) snapshot the live frame
    // stack as the enclosing scope. bMakePrototype builds + installs a fresh
    // AptPrototype object member.
    AptScriptFunctionBase(AptVirtualFunctionTable_Indices eType,
                          AptValue* pCallContext,
                          AptValue* pCIH,
                          bool      bMakePrototype);

    // copy ctor @0x82B00E90 -- duplicate rOther's scope state (proto / __proto__ /
    // parent scope) while re-binding to pCIH. Used by the subclass Duplicate paths.
    AptScriptFunctionBase(AptVirtualFunctionTable_Indices eType,
                          const AptScriptFunctionBase& rOther,
                          AptValue* pCIH);

    virtual ~AptScriptFunctionBase();

    // +0x20 -- the bound character-instance-handle value (AddRef'd; "CIH").
    AptValue* mpCIH;
    // +0x24 -- the timeline animation this function was defined on ("ParentAnim").
    AptValue* mpParentAnim;
    // +0x28 -- the captured enclosing call frame ("ParentScope"); null at top level.
    AptValue* mpParentScope;
    // +0x2C -- non-zero while a nested DefineFunction is mid-construction.
    uint16_t  mnCreatingNestedFunction;

    // ---- process-wide AS execution state (X360 .data globals) ------------------
    // The current local-variable frame stack (off_8324E3DC); CreateFrameStack
    // installs it and SetArgument inserts into its locals.
    static AptFrameStack* spFrameStack;
    // The flat AS register array base (off_8324E3D0) + the live-register count
    // high-water mark (dword_8324E3D4) + the allocated capacity (dword_8324E3D8,
    // used by ShutdownStaticData to free the array).
    static AptValue**     spRegisters;
    static int32_t        snRegisterCount;
    static int32_t        snRegisterCapacity;
    // The register-BLOCK frame stack (X360 spRegBlockCurrentFrameBase /
    // snRegBlockCurrentFrameCount): a moving base into the register block carved from
    // the arg heap. PushStaticData grows it; PopStaticData pops it back to a saved base.
    static AptValue**     spRegBlockCurrentFrameBase;
    static int32_t        snRegBlockCurrentFrameCount;
};
