// ===========================================================================
// EATech Apt -- AptInit.cpp: the Apt runtime BRING-UP entry points.
//
// Decompiled store-for-store from the X360 ARTIST.XEX (the `assembly` field is
// authoritative for the call order + which register/global each store touches).
// These are the functions CgsGui::AptAux::InitializeApt @0x82848E50 chains to
// stand up the Apt runtime; the GUI Apt host (BrnGuiAptRuntime.cpp) previously
// stood these pieces up with an invented facade, which this TU retires.
//
// PHYSICALLY-REQUIRED PC DEVIATIONS (each // FLAG'd at its site):
//   * The X360 brackets its global-state mutations with an interrupt-disabling
//     lwarx/stwcx. test-and-set spin lock (the mfmsr/mtmsree idiom). On the
//     single-threaded PC bring-up path there is no contention; the acquire/release
//     is a no-op and is elided (the same treatment AptTarget.cpp / AptStringPool.cpp
//     already apply to the Apt spin locks). EA::Thread::GetThreadId still runs.
//   * The console stores 4-byte pointers into its .data slots; on x64 those slots
//     hold sizeof(ptr) -- the pervasive Apt x64-port rule. All storage here is
//     x64-native (the globals are C++ objects, not a serialised image).
//   * StringPool::Initialize populates the interned AS-name table from an
//     un-recovered rodata block (unk_82F733FC); that string-content copy is FLAG'd
//     (matching the AptGlobals.cpp FLAG on gAptASNameTable) while the structural
//     pool-array allocation stays faithful.
//   * AptValueInitialize @0x82B02800 (the AS value-singleton bootstrap) is
//     FLAG-DEFERRED, not homed here: its ~15 value ctors (AptNone/AptBoolean::
//     Initialize/AptCIHNone/AptKey/...) are `protected` in the reconstructed types
//     (they are engine-internal singletons the class hierarchy constructs, not an
//     external TU), so a faithful call cannot be made without befriending it into
//     each class. The singletons stay null (their documented pre-init state -- see
//     AptGlobals.cpp), and the runtime stays stable; see the AptValueInitialize FLAG.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/Apt/AptInit.h"

#include <cstring>   // memcpy / memset
#include <cstdint>   // intptr_t
#include <new>       // placement new (AptTarget)

// ---- the shared fixed-size pool + the value pools --------------------------
#include "SDKs/EATech/Apt/DogmaAllocator.h"                     // DOGMA_PoolManager::Allocate / DOGMA_FreeSized
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"              // AptValueGC_PoolManager + gAptValueGC*ItemSize
#include "SDKs/EATech/include/Apt/AptDefine.h"                  // gpNonGCPoolManager / gpGCPoolManager

// ---- the leaves AptCommonInitialize calls ----------------------------------
#include "SDKs/EATech/Apt/AptMath.h"                            // AptMath::ClipStackInit
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"         // AptAnimationTarget::SetupStaticData
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"    // AptValueVector (ctor)
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"       // AptActionInterpreter::initialize + InitDispatchTable + AptInitParmsT

// ---- the value singletons AptValueInitialize builds -------------------------
#include "SDKs/EATech/include/Apt/AptValue/AptNone.h"           // AptNone (the `undefined` singleton; befriended)
#include "SDKs/EATech/include/Apt/AptExtObject.h"              // CreateNewAptFunction (the registerClass native wrap)
#include "SDKs/EATech/include/Apt/AptObject.h"                 // AptObject::RegisterClassNative (registerClass native)
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"         // AptNativeFunction (complete type for the AptValue* store)
#include "SDKs/EATech/include/Apt/AptNativeHash.h"             // the _global hash (builtin class install)
#include "SDKs/EATech/include/Apt/AptPrototype.h"               // AptPrototype (builtin prototype seeding)
#include "SDKs/EATech/include/Apt/AptCIHNativeFunctionHelper.h" // the MovieClip prototype's native methods
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"        // AptBoolean::Initialize/Shutdown (befriended)
#include "SDKs/EATech/include/Apt/AptValue/AptLookup.h"         // AptLookup::Initialize/Shutdown
#include "SDKs/EATech/include/Apt/AptValue/AptRegister.h"       // AptRegister::Initialize/Shutdown + gnAptRegisterCount
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"        // AptInteger::ClearPool (befriended for AptCommonShutdown)
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"          // AptFloat::ClearPool (befriended for AptCommonShutdown)
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"         // AptCharacterHelper::Shutdown (AptValueShutdownRemaining)
#include "SDKs/EATech/include/Apt/AptCIHNone.h"                 // AptCIHNone (the "EmptyCIH" placeholder; befriended)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"         // EAStringC ("EmptyCIH" name assignment)
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"        // AptRenderingContext (the shared render context)
#include "SDKs/EATech/include/Apt/AptValue/AptExtern.h"         // AptExtern (the extern-interface singleton)
#include "SDKs/EATech/include/Apt/AptKey.h"                     // AptKey (the Key manager singleton)
#include "SDKs/EATech/include/Apt/AptGlobal.h"                  // AptGlobal (the _global fallback scope)
#include "SDKs/EATech/include/Apt/AptGlobalExtensionObject.h"   // AptGlobalExtensionObject (_global extension)
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"         // AptString::Create (the shared empty string)

// StringPool::Initialize is reached through this free wrapper (AptStringPool.cpp): the
// full StringPool.h cannot be included here (the interpreter headers already carry
// AptNativeHash.h's incompatible mini `class StringPool`).
void AptStringPool_Initialize(int nBucketCount);   // AptStringPool.cpp (== StringPool::Initialize @0x82AE3630)
void AptStringPool_Teardown();                     // AptStringPool.cpp (== StringPool::Teardown @0x82AE3720)
void AptStringPool_ClearTemporaryPool();           // AptStringPool.cpp (== StringPool::ClearTemporaryPool @0x82AD8E20)

// ---- the AptTarget context (AptUpdateInitialize's a2 branch) ---------------
#include "SDKs/EATech/include/Apt/AptTarget.h"                  // AptTarget (ctor) + AptChangeTargetInstance

// ---- the shutdown-teardown leaves AptUpdateShutdown chains through ----------
#include "SDKs/EATech/include/Apt/AptArray.h"                   // AptArray::CleanNativeFunctions
#include "SDKs/EATech/include/Apt/AptScriptColour.h"            // AptScriptColour::CleanNativeFunctions
#include "SDKs/EATech/include/Apt/AptCIH.h"                     // AptCIH::CleanNativeFunctions
#include "SDKs/EATech/include/Apt/AptError.h"                   // AptError::CleanNativeFunctions
#include "SDKs/EATech/include/Apt/AptGC.h"                      // AptGC::CleanAll

// ---- EA::Thread (record the sim/render thread ids + the shared mutex) -------
#include "eathread/eathread.h"                                  // EA::Thread::GetThreadId / ThreadId
#include "eathread/eathread_mutex.h"                            // EA::Thread::Mutex (vendor)
#include "SDKs/EATech/eathread/thread_local_storage.h"          // EA::Thread::ThreadLocalStorage (gAptTargetTls)

// ===========================================================================
// The Apt bring-up .data/.bss globals this TU owns (the X360 objects the init
// routines populate). Faithful pre-init state: 0 / null. Named after their
// X360 symbol; access is by name.
// ===========================================================================

// The 68-byte shared config block (unk_82F733B8) -- AptUpdateInitialize fills it,
// AptCommonInitialize + AptActionInterpreter::initialize read it. 17 dwords so
// byte[64] (the interpreter's skip-trace flag at config +0x40) is addressable.
unsigned int gAptCommonConfig[17] = { 0 };   // unk_82F733B8

namespace
{
    // ---- the once-only init guard (dword_8324E6E0) -------------------------
    int gbAptCommonInitDone = 0;   // dword_8324E6E0
    // ---- the render / sim "initialized" latches ----------------------------
    int gbAptRenderInitDone = 0;   // dword_8324E510
    int gbAptUpdateInitDone = 0;   // dword_8324E50C

    // ---- the sim thread id (dword_8324E500). EA::Thread::ThreadId is pointer-
    //      sized. The RENDER id (dword_8324E504) is owned by AptGlobals.cpp
    //      (gAptRenderThreadId) since AptTarget.cpp reads it there.
    EA::Thread::ThreadId gAptSimThreadId = EA::Thread::ThreadId();   // dword_8324E500

    // ---- the render-item pointer pool (off_8324E2C8) -----------------------
    void* gpAptRenderItemPool = nullptr;   // off_8324E2C8

    // ---- the shared deferred-release AptValueVector pointers ---------------
    AptValueVector* gpAptDeferredVecCommon = nullptr;   // off_8324E51C (common-init side)

    // ---- the AptUpdateInitialize single-list free node (dword_8324D810) ----
    void* gpAptUpdateListNode = nullptr;   // dword_8324D810

    // ---- config-derived flag byte (byte_8324E393) --------------------------
    unsigned char gbAptUpdateFlag393 = 0;   // byte_8324E393

    // ---- off_82F733B0 / unk_82F72FF8 (the config "empty default" sentinel) --
    unsigned char gAptEmptyDefaultSentinel[8] = { 0 };   // unk_82F72FF8
    void*         gpAptConfigDefault = nullptr;           // off_82F733B0

    // ---- dword_8324E4F0 (the common-init gAptFuncs self-registration slot) -
    void* gpAptCommonFuncsSlot = nullptr;   // dword_8324E4F0

    // (the string-pool bucket array off_8324E4F4 / count dword_8324E4F8 are owned by
    //  AptStringPool.cpp, where StringPool::Initialize lives.)

    // ---- the shared thread-id spin lock (unk_8324E724) ---------------------
    // FLAG PC-platform leaf: single-threaded thread-id spin lock -- the console's interrupt-masked lwarx/stwcx. TAS elided (threading primitive, not an engine method).
    inline void AptThreadIdLock_Acquire() {}
    inline void AptThreadIdLock_Release() {}

    // ---- the Apt update/render shared mutex (unk_8324E7E0) ------------------
    // Vendor EA::Thread::Mutex (default recursive). FLAG: the console passes a raw
    // name (unk_82143270 == the MutexParameters); a default-constructed recursive
    // mutex is the faithful shape. Function-local static -> init-order safe.
    EA::Thread::Mutex& AptUpdateRenderMutex()
    {
        static EA::Thread::Mutex s_mutex;   // unk_8324E7E0
        return s_mutex;
    }

    // ---- the deferred-release drain mutex (unk_8324E728) -------------------
    // A SECOND vendor EA::Thread::Mutex, distinct from the update/render mutex
    // above; AptRenderShutdown takes it to drain the render-item / deferred-
    // release pool (off_8324E2C8 / dword_8324E508) -- the same lock the AS
    // unresolve path uses. Same function-local-static shape as
    // AptUpdateRenderMutex (init-order safe). FLAG: the console passes the raw
    // MutexParameters name (unk_82143270); a default recursive mutex is faithful.
    // (AptGlobals.cpp keeps a null `void* gAptUnresolveMutex` symbol placeholder;
    // the functional object is modelled here, mirroring the render mutex.)
    EA::Thread::Mutex& AptUnresolveMutex()
    {
        static EA::Thread::Mutex s_mutex;   // unk_8324E728
        return s_mutex;
    }

    // The DOGMA sized-free hook slot's signature is (void*, unsigned) while
    // DOGMA_FreeSized takes size_t; a thin adapter forwards it (x64: unsigned !=
    // size_t -- a straight fn-ptr assign would not type-check).
    void AptDogmaFreeSizedHook(void* pBlock, unsigned nBytes)
    {
        DOGMA_FreeSized(pBlock, static_cast<size_t>(nBytes));
    }
} // namespace

// The shared fixed-size pool handle (off_8324D808). Defined in AptGlobals.cpp.
extern DOGMA_PoolManager* gpAptPseudoDataPool;   // off_8324D808

// The DOGMA sized-free hook (dword_8324E820) the update-init installs when null.
extern void (*gpAptGCTableFree)(void* p, unsigned nBytes);   // dword_8324E820 (AptGlobals.cpp)

// byte_8324E392 -- the mouse-wheel default flag (AptGlobals.cpp).
extern bool gAptDefaultTextMouseWheelEnabled;   // byte_8324E392

// dword_8324E504 -- the render-thread id (AptGlobals.cpp; AptTarget.cpp reads it).
extern void* gAptRenderThreadId;   // dword_8324E504 (EA::Thread::ThreadId == void*)

// The interpreter VM singleton (dword_8324E760). Defined in AptGlobals.cpp.
extern AptActionInterpreter gAptActionInterpreter;

// gAptFuncs (dword_8324E818) -- the host user-function table, defined in CgsAptAux.cpp.
struct AptUserFunctions;
extern AptUserFunctions gAptFuncs;

// The Apt pool-pointer globals AptAllocatorInitialize wires (all alias off_8324D808;
// the GC-view is off_8324D834). Defined in AptGlobals.cpp; wired here.
extern DOGMA_PoolManager* gpAptOperandStackPool;   // off_8324D808 (operand-stack arrays)
extern DOGMA_PoolManager* gpAptRenderManagerPool;  // off_8324D808
extern DOGMA_PoolManager* gpAptSharedPtrPool;      // off_8324D808
extern DOGMA_PoolManager* gpAptSingleListPool;     // off_8324D808
extern void*              gpAptValueGCPool;        // off_8324D834 (type-erased GC-pool view)

// ---------------------------------------------------------------------------
// The Apt TEARDOWN leaves AptRenderShutdown reaches. The un-homed siblings are
// declared here at the call site (the codebase pattern for not-yet-homed deps).
// ---------------------------------------------------------------------------
// AptMath::ClipStackShutdown -- free the render clip stack (the counterpart of
// ClipStackInit @0x82AE2470); returns the freed base in the console r3, so
// intptr_t on x64 (matching the other ClipStack* accessors). Not yet homed.
namespace AptMath { intptr_t ClipStackShutdown(); }

// AptCommonShutdown -- the once-only shared static-data teardown (counterpart of
// AptCommonInitialize @0x82AE91F0). Recovered signature takes no args; the X360
// leaves ClipStackShutdown's r3 result live across the call (a dead leftover --
// the emitted machine code is identical either way). HOMED below (@0x82B0AC08);
// forward-declared here for AptRenderShutdown, which precedes the definition.
int AptCommonShutdown();

// The render-item / deferred-release pool count (dword_8324E508, homed in
// AptGlobals.cpp) + the host font-free thunk (dword_8324E870, a member of the
// gAptFuncs table, named beside the AS unresolve path in AptCharacterAnimation.cpp)
// the shutdown drain calls on each pooled slot.
extern int  gAptDeferredReleaseCount;       // dword_8324E508 (AptGlobals.cpp)
extern void AptFreeFontUnit(void* pUnit);   // dword_8324E870 thunk

// ===========================================================================
// AptAllocatorInitialize @0x82ADD118 -- construct the Apt value pools + wire the
// pool-pointer globals.
//
// X360: AptValueGC_PoolManager::StaticInitialize(); mem = dword_8324E818(48) [the base
// allocator hook]; off_8324D808 = DOGMA_PoolManager(mem, dogmaMain, dogmaOvf, 4, 256,
// 0, 0, 0); mem2 = dword_8324E818(48); off_8324D834 = AptValueGC_PoolManager(mem2,
// gcMain, gcOvf). AptAux::InitializeApt calls it (0x10000, 0x4000, 0x10000, 0x4000).
//
// FLAG (PC): the base allocator hook dword_8324E818(48) is not installed at bring-up,
// so the two managers are backed by process-lifetime static storage (same lifetime as
// the console heap pools). FLAG (x64): byte_82144A18 is zeroed so StaticInitialize
// leaves the GC maxSize 0 -> override the GC size statics with x64-correct values
// (4/256) before the GC pool ctor reads them, else AptValue allocs take the invalid
// 0-bucket path and AV. WireAllocatorGlobals sets off_8324D808/off_8324D834 (+ the
// operand/pseudo/render/shared/single-list aliases the engine reads off the non-GC
// pool). The DOGMA fixed-size params (minSize 4, maxSize 256, 0/0/0,
// bTrackOutsideAllocations 1) are transcribed from the @0x82ADD118 asm.
// ===========================================================================
namespace
{
    DOGMA_PoolManager*      s_pDogmaPool = nullptr;   // off_8324D808
    AptValueGC_PoolManager* s_pGCPool    = nullptr;   // off_8324D834

    void WireAllocatorGlobals()
    {
        // The five operand/pseudo/render/shared/single-list pool aliases all point at
        // the one shared DOGMA pool (faithful: X360 off_8324D808).
        gpAptOperandStackPool  = s_pDogmaPool;
        gpAptPseudoDataPool    = s_pDogmaPool;
        gpAptRenderManagerPool = s_pDogmaPool;
        gpAptSharedPtrPool     = s_pDogmaPool;
        gpAptSingleListPool    = s_pDogmaPool;

        // The non-GC value pool (AptDefine.h gpNonGCPoolManager) also aliases the DOGMA
        // pool; the GC value pool pointer (gpGCPoolManager) points at the AptValueGC pool.
        gpNonGCPoolManager = s_pDogmaPool;
        gpGCPoolManager    = s_pGCPool;

        // The type-erased GC-pool view the engine stamps (off_8324D834).
        gpAptValueGCPool = s_pGCPool;
    }
}

void* AptAllocatorInitialize(int nGcMain, int nGcOvf, int nDogmaMain, int nDogmaOvf)
{
    AptValueGC_PoolManager::StaticInitialize();
    gAptValueGCMinItemSize = 4u;      // FLAG (x64): override the byte_82144A18-derived 0
    gAptValueGCMaxItemSize = 256u;    // FLAG (x64): "                                   "
    static DOGMA_PoolManager s_DogmaStorage(nDogmaMain, nDogmaOvf,
                                            /*minSize*/ 4, /*maxSize*/ 256,
                                            /*nOffsetToStoreNextInFreeItem*/ 0,
                                            /*bStoreFreeBlockSize*/ false,
                                            /*nOffsetToStoreSizeInFreeItem*/ 0,
                                            /*bTrackOutsideAllocations*/ true);   // FLAG (PC): static backing
    s_pDogmaPool = &s_DogmaStorage;
    static AptValueGC_PoolManager s_GCStorage(nGcMain, nGcOvf);   // FLAG (PC): static backing
    s_pGCPool = &s_GCStorage;
    WireAllocatorGlobals();
    return s_pGCPool;
}

// ===========================================================================
// AptSetSimulationThreadID @0x82AD8F90 / AptSetRenderThreadID @0x82AD8EF0
//
// X360: interrupt-masked TAS on unk_8324E724; id = EA::Thread::GetThreadId(a1);
// store into the sim (dword_8324E500) / render (dword_8324E504) slot; unlock.
//
// FLAG (PC): the console's GetThreadId(a1) takes a thread-handle arg; the vendor
// EA::Thread::GetThreadId() is the no-arg "current thread" form (the value the
// console yields here -- these are called on the main thread with a1 that resolves
// to the caller). `a1` is accepted for signature parity but unused (the vendor has
// no by-handle GetThreadId on this build).
// ===========================================================================
int AptSetSimulationThreadID(int /*a1*/)
{
    AptThreadIdLock_Acquire();                 // FLAG: interrupt-masked TAS elided
    EA::Thread::ThreadId id = EA::Thread::GetThreadId();
    gAptSimThreadId = id;                       // dword_8324E500 = result
    AptThreadIdLock_Release();
    return static_cast<int>(reinterpret_cast<intptr_t>(id));
}

int AptSetRenderThreadID(int /*a1*/)
{
    AptThreadIdLock_Acquire();                 // FLAG: interrupt-masked TAS elided
    EA::Thread::ThreadId id = EA::Thread::GetThreadId();
    gAptRenderThreadId = id;                    // dword_8324E504 = result
    AptThreadIdLock_Release();
    return static_cast<int>(reinterpret_cast<intptr_t>(id));
}

// ===========================================================================
// AptCommonInitialize @0x82AE91F0 -- the once-only shared static-data init.
//
// X360: interrupt-masked TAS set of dword_8324E6E0 (==1); AptAnimationTarget::
// SetupStaticData(pConfig); dword_8324E4F0 = &dword_8324E818 (the gAptFuncs pointer
// self-registration slot); StringPool::Initialize(pConfig[+0x2C]); mem =
// pool->Allocate(12); if (mem) off_8324E51C = AptValueVector(mem, pConfig[+0x28]).
//
// The Hex-Rays "SetupStaticData(a1,a2,a3,a4,a5)" is spurious (leftover register
// signature); the asm passes ONLY r3 = pConfig. SetupStaticData reads its count from
// pConfig[+0x18] (word[6]); the homed SetupStaticData takes that count directly.
// ===========================================================================
void* AptCommonInitialize(void* pConfig)
{
    unsigned int* lpCfg = static_cast<unsigned int*>(pConfig);

    // FLAG: the console's interrupt-masked TAS on dword_8324E6E0 is elided (single-
    // threaded). Setting the guard (=1) is the faithful visible result.
    gbAptCommonInitDone = 1;

    // AptAnimationTarget::SetupStaticData(pConfig) -- count == pConfig word[6] (+0x18).
    AptAnimationTarget::SetupStaticData(static_cast<int>(lpCfg[6]));

    // dword_8324E4F0 = &gAptFuncs (dword_8324E818): the common-init self-registers
    // the host user-function table pointer.
    gpAptCommonFuncsSlot = &gAptFuncs;

    // StringPool::Initialize(pConfig word[11] == +0x2C): the string-pool bucket count.
    // (reached via the free wrapper -- see the AptStringPool_Initialize decl above.)
    AptStringPool_Initialize(static_cast<int>(lpCfg[11]));

    // off_8324E51C = AptValueVector(pool->Allocate(12), pConfig word[10] == +0x28).
    // (The console's 12-byte alloc is the AptValueVector {cap,top,items} header; on x64
    //  that header is sizeof(AptValueVector) -- allocate that. The ctor then allocates
    //  the item array from the operand-stack pool.)
    void* lpVecMem = gpAptPseudoDataPool->Allocate(sizeof(AptValueVector));   // console 12; x64 sizeof
    if (lpVecMem != nullptr)
    {
        AptValueVector* lpVec = static_cast<AptValueVector*>(lpVecMem);
        *lpVec = AptValueVector::ConstructAptValueVector(static_cast<int>(lpCfg[10]));
        gpAptDeferredVecCommon = lpVec;
        return lpVec;
    }
    gpAptDeferredVecCommon = nullptr;
    return nullptr;
}

// ===========================================================================
// AptValueInitialize @0x82B02800 -- the AS value-singleton bootstrap.
//
// The console builds the AS value singletons here and drains the common-init
// deferred-release vector. WIRED (2026-07-01), in the console's exact leading order:
//   1. off_8324D814 (gpUndefinedValue) = a pooled AptNone -- the shared `undefined`
//      (X360: Allocate(off_8324D808, 8) then AptNone::AptNone(); the pooled operator
//      new + the befriended protected ctor reproduce that on x64 widths).
//   2. AptBoolean::Initialize()  (the shared true/false pair; befriended)
//   3. AptLookup::Initialize()   (the slot-indexed lookup table)
//   4. AptRegister::Initialize() (the AS register-value file; reads gnAptRegisterCount,
//      published from the config block by AptUpdateInitialize before this runs)
//   5. gpAptEmptyCIH (dword_8324D700) = a pinned AptCIHNone named "EmptyCIH" (the
//      absent-CIH placeholder handle; befriended)
// FLAG (deferred -- the remaining singletons): AptRenderingContext, AptExtern, AptKey,
// AptGlobal(+ExtensionObject), the AptString empties and the 7 AS native-function
// singletons still have protected bootstraps not yet exposed to this TU; they keep
// their documented null pre-init state (the engine short-circuits on null) until each
// is befriended/homed. Returns 0 (the console's r3 == the last ReleaseValues result).
// ===========================================================================
extern AptValue* gpUndefinedValue;            // off_8324D814 (defined in AptGlobals.cpp)
extern AptCIH*   gpAptEmptyCIH;               // dword_8324D700 (defined in AptGlobals.cpp)
extern void*     gpAptRenderingContext;       // dword_8324E2AC (defined in AptGlobals.cpp)
extern AptValue* gpAptExternObject;           // off_8324E2CC  ("")
extern AptValue* gpAptKeyObject;              // off_8324E2A8  ("")
// (gpAptGlobalFallback off_8324E380 is declared by AptGlobal.h, included above)
extern AptValue* gpAptGlobalExtensionObject;  // off_8324E37C  ("")
extern AptValue* gpAptStringObject;           // off_8324D82C  ("")
extern AptValue* gpObjRegistrationFunc;       // off_8324D748  (the Object.registerClass native fn)
extern AptValue* gpAptFunctionPrototypeRoot;  // dword_8324E4EC (the builtin prototype root)
extern const EAStringC gAptObjectClassName;   // &dword_8324E650 "Object"
extern const EAStringC gAptStringClassName;   // &dword_8324E6B4 "String"
extern const EAStringC gAptSpriteClassKey;    // dword_8324E640 "MovieClip"

// The registerClass native is AptObject::RegisterClassNative (AptObject.cpp;
// X360 sub_82AF6A38); its declaration arrives with AptObject.h below.

int AptValueInitialize()
{
    if (gpUndefinedValue == nullptr)
        gpUndefinedValue = new AptNone();   // pooled operator new (gpNonGCPoolManager)

    AptBoolean::Initialize();
    AptLookup::Initialize();
    AptRegister::Initialize();

    // dword_8324D700 -- the shared "EmptyCIH" placeholder handle: a pinned AptCIHNone
    // (X360: AptCIH::operator new(40) + AptCIHNone(); then InitFromBuffer("EmptyCIH")
    // assigned into the node's instance-name slot [+8] with the RAII temp released).
    if (gpAptEmptyCIH == nullptr)
    {
        gpAptEmptyCIH = new AptCIHNone();
        gpAptEmptyCIH->mInstanceName = EAStringC("EmptyCIH");
    }

    // dword_8324E2AC -- the shared AptRenderingContext (console Allocate(off_8324D808,
    // 1096) + placement ctor; x64 uses sizeof over the same pool).
    if (gpAptRenderingContext == nullptr)
    {
        void* lpBlock = gpAptPseudoDataPool->Allocate(sizeof(AptRenderingContext));
        gpAptRenderingContext = lpBlock ? new (lpBlock) AptRenderingContext() : nullptr;
    }

    // off_8324E2CC -- the extern-interface singleton (console Allocate(8) + placement;
    // the class's pooled operator new reproduces it).
    if (gpAptExternObject == nullptr)
        gpAptExternObject = new AptExtern();

    // off_8324E2A8 -- the Key manager singleton (+AddRef, faithful to the asm's vtbl[0]).
    if (gpAptKeyObject == nullptr)
    {
        gpAptKeyObject = new AptKey();
        gpAptKeyObject->AddRef();
    }

    // off_8324E380 -- the _global fallback scope (+AddRef). The console wraps these in
    // interlocked one-shot latches (unk_8324E71C/..E720); single-threaded init here, so
    // the idempotent null-checks are the equivalent one-time guard.
    if (gpAptGlobalFallback == nullptr)
    {
        gpAptGlobalFallback = new AptGlobal();
        gpAptGlobalFallback->AddRef();
    }

    // off_8324E37C -- the _global extension object (+AddRef).
    if (gpAptGlobalExtensionObject == nullptr)
    {
        AptGlobalExtensionObject* const pExt = new AptGlobalExtensionObject();
        gpAptGlobalExtensionObject = pExt;
        gpAptGlobalExtensionObject->AddRef();
        // off_8324E37C aliases: AptGlobal::objectMemberLookup consults the same
        // console address through the AptValueWithHash*-typed gpAptNativeGlobals
        // (AptGlobal.cpp) -- keep the two views pointing at the ONE object.
        gpAptNativeGlobals = pExt;
    }

    // off_8324D82C -- the shared empty AptString (Create(&unk_820046A7) == Create("")).
    if (gpAptStringObject == nullptr)
        gpAptStringObject = AptString::Create("");

    // The Object.registerClass native (off_8324D748 == gpObjRegistrationFunc): the
    // console's tail calls sub_82AF6B68, which -- among the builtin prototype seeding
    // -- wraps the registerClass native (sub_82AF6A38, homed as
    // AptObject::RegisterClassNative in AptObject.cpp) in an AptNativeFunction and pins
    // it. Installed here (the same boot moment); FLAG: the REST of sub_82AF6B68 (the
    // per-builtin-class AptPrototype seeding over _global's hash entries) stays
    // deferred with the AS-builtin class registry.
    if (gpObjRegistrationFunc == nullptr)
    {
        gpObjRegistrationFunc = AptExtObject::CreateNewAptFunction(
            reinterpret_cast<AptExtFunctionPtr>(&AptObject::RegisterClassNative));
        if (gpObjRegistrationFunc)
            gpObjRegistrationFunc->setGCRoot(1);
    }

    // ---- sub_82AF6B68 phases 1+2, the NAMED slice --------------------------------
    // The console's AS-globals bootstrap installs 9 builtin class natives into
    // _global's hash (off_8324E380+8) -- each a fresh AptNativeFunction wrapping the
    // SHARED generic-constructor stub (the Hex-Rays resolve of every one of the nine
    // is cbCallMethod_ASSetPropFlags @0x82AD8448, the return-ok stub; the classes'
    // real behaviour lives in the prototypes + the engine value types) -- then seeds
    // each with a fresh AptPrototype: the FIRST entry ("Object", &dword_8324E650)
    // becomes the prototype ROOT (dword_8324E4EC == gpAptFunctionPrototypeRoot; the
    // console also mirrors it into dword_8324D830), and every entry's own __proto__
    // links to that root. FLAG (partial): only the three entries whose name constants
    // are recovered ("Object" E650 / "String" E6B4 / "MovieClip" E640) are installed;
    // the six remaining table names (unk_8324E5FC/E610/E618/E6BC/E6D4/E61C) await the
    // rodata string extraction and stay uninstalled. `Object` is the one MAIN's
    // framework bootstrap needs (Object.registerClass reaches the native through the
    // inherited AptObject::objectMemberLookup).
    if (gpAptGlobalFallback != nullptr)
    {
        AptNativeHash* const pGlobals = gpAptGlobalFallback->GetNativeHashVirtual();
        if (pGlobals != nullptr && pGlobals->Lookup(gAptObjectClassName) == nullptr)
        {
            const EAStringC* const lakNames[3] =
                { &gAptObjectClassName, &gAptStringClassName, &gAptSpriteClassKey };
            for (int li = 0; li < 3; ++li)
            {
                AptNativeFunction* const pCtor = AptExtObject::CreateNewAptFunction(
                    reinterpret_cast<AptExtFunctionPtr>(
                        &AptActionInterpreter::cbCallMethod_ASSetPropFlags));
                if (pCtor)
                    pGlobals->Set(*lakNames[li], pCtor);
            }

            // The console resolves Object.registerClass through AptObject::
            // objectMemberLookup (every AptObject-family value knows the name);
            // our AptNativeFunction hierarchy sits beside AptObject, so install
            // the native as an ORDINARY member of the Object builtin's hash --
            // observable-identical for AS lookups. FLAG: revisit if the value
            // class tree is re-based onto AptObject.
            {
                AptValue* const pObjectClass = pGlobals->Lookup(gAptObjectClassName);
                AptNativeHash* const pObjHash =
                    pObjectClass ? pObjectClass->GetNativeHashVirtual() : nullptr;
                if (pObjHash && gpObjRegistrationFunc)
                {
                    EAStringC lRegKey("registerClass");
                    pObjHash->Set(lRegKey, gpObjRegistrationFunc);
                }
            }

            for (int li = 0; li < 3; ++li)
            {
                AptValue* const pClass = pGlobals->Lookup(*lakNames[li]);
                if (pClass == nullptr)
                    continue;
                AptPrototype* const pProto = new AptPrototype();
                AptNativeHash* const pClassHash = pClass->GetNativeHashVirtual();
                if (pClassHash != nullptr)
                {
                    pClassHash->SetPrototype(pProto);
                    if (li == 0)
                        gpAptFunctionPrototypeRoot = pProto;   // dword_8324E4EC (the root)
                    else
                        pClassHash->Set__Proto__(gpAptFunctionPrototypeRoot);
                }
                pClass->setGCRoot(1);
                if (pProto)
                    pProto->setGCRoot(1);
                // (The MovieClip clip METHODS -- gotoAndPlay / play / getTextFormat /
                // ... -- are NOT prototype members: the console resolves them through
                // AptCIH::objectMemberLookup's SpriteMembersIndex recognizer, homed in
                // AptCIHMembers.cpp 2026-07-09. The interim prototype install this TU
                // carried was retired with that landing.)
            }
        }
    }

    // FLAG (deferred): the remaining singletons -- the 7 AS native-function singletons
    // (setInterval/clearInterval/isNaN/unescape/escape/boolean/ASSetPropFlags) + the
    // SIX un-named builtin table entries of sub_82AF6B68 -- stay null until homed.
    return 0;
}

// ===========================================================================
// AptValueShutdownRemaining @0x82AE3170 -- release the `undefined` singleton, tear
// down the boolean/lookup/register/character-helper static pools, then release the
// extern-interface singleton (the counterpart of the corresponding AptValueInitialize
// bring-up steps).
//
// X360 (store-for-store): off_8324D814->ForceDelete() [vtbl +0x2C]; AptBoolean::
// Shutdown(); AptLookup::Shutdown(); AptRegister::Shutdown(); AptCharacterHelper::
// Shutdown(); return off_8324E2CC->ForceDelete() [vtbl +0x2C]. The r3 the four static
// Shutdowns thread through is dead register plumbing (each is void).
//
// AptBoolean::Shutdown is `protected` in AptBoolean.h (AptLookup/AptRegister/
// AptCharacterHelper::Shutdown are public); AptBoolean.h now grants this function an
// additive `friend int ::AptValueShutdownRemaining();`, symmetric to the existing
// AptValueInitialize friend, so the protected call is legal. The console tail-returns
// the second ForceDelete's r3; ForceDelete is `virtual void`, so the int return is a
// dead leftover -- 0 on the vendor path (matching AptValueShutdown's treatment).
// ===========================================================================
int AptValueShutdownRemaining()
{
    // off_8324D814 -- the shared `undefined` (AptNone) singleton: virtual ForceDelete
    // (vtbl +0x2C) runs its PreDestroy/DestroyGCPointers + `delete this` back to the pool.
    gpUndefinedValue->ForceDelete();

    // Tear down the four value-pool statics (each frees + clears its pinned pool/pair).
    AptBoolean::Shutdown();          // the true/false singleton pair (befriended)
    AptLookup::Shutdown();           // the slot-indexed lookup pool
    AptRegister::Shutdown();         // the AS register file
    AptCharacterHelper::Shutdown();  // the default text/movie character templates

    // off_8324E2CC -- the extern-interface singleton: ForceDelete (vtbl +0x2C). The
    // console tail-returns its r3; the virtual is void here, so 0 (the vendor result).
    gpAptExternObject->ForceDelete();
    return 0;
}

// ===========================================================================
// AptValueShutdown @0x82AF2BE0 -- tear down the AS value singletons + natives.
//
// The teardown mirror of AptValueInitialize: drop the AS class-registry hash, then
// walk the process-wide value singletons -- for each, run the value's GC-pointer
// teardown (vtbl DestroyGCPointers @+0x28) then the boot-AddRef Release (vtbl Release
// @+0x04) and null the slot. The rendering context (a pooled AptRenderingContext, not
// an AptValue) is torn down via its scalar-deleting destructor and its slot is left
// as-is (the console does NOT null dword_8324E2AC here). Called by AptUpdateShutdown.
//
// FLAG (PC): the console brackets the walk with two interrupt-masked lwarx/stwcx. TAS
// spin locks on unk_8324E71C (the same one-shot latch AptValueInitialize uses);
// single-threaded here, so acquire/release is a no-op and elided (the AptInit.cpp
// spin-lock treatment). No null guards are added: the console's per-singleton
// dispatch is unconditional (only dword_8324E2D4 and dword_8324E2AC are guarded),
// transcribed verbatim.
//
// FLAG (deferred): the seven AS native-function singletons (off_8324D828/D81C/D824/
// E1FC/D80C/D74C/E378) are the teardown side of the SAME singletons AptValueInitialize
// FLAG-defers (they stay null until that native-install slice is homed). Their slots
// live below as this-TU storage so the faithful walk resolves; the whole
// AptValueShutdown path stays inert until those natives (and AptUpdateShutdown, its
// caller) are homed.
// ===========================================================================
extern AptNativeHash* gpAptClassRegistry;   // dword_8324E2D4 (AptObject.cpp)

namespace
{
    // The seven named AS global native-function singletons (X360 off_8324D8xx /
    // off_8324E1FC / off_8324E378). Deferred-null (see AptValueInitialize's FLAG);
    // this TU owns their slots for the init/teardown pair. FLAG: file-local until the
    // deferred native-install slice homes them alongside the other Apt singletons.
    AptValue* gpAptNativeFn_setInterval    = nullptr;   // off_8324D828
    AptValue* gpAptNativeFn_clearInterval  = nullptr;   // off_8324D81C
    AptValue* gpAptNativeFn_isNaN          = nullptr;   // off_8324D824
    AptValue* gpAptNativeFn_unescape       = nullptr;   // off_8324E1FC
    AptValue* gpAptNativeFn_escape         = nullptr;   // off_8324D80C
    AptValue* gpAptNativeFn_boolean        = nullptr;   // off_8324D74C
    AptValue* gpAptNativeFn_ASSetPropFlags = nullptr;   // off_8324E378
}

int AptValueShutdown()
{
    // dword_8324E2D4 (gpAptClassRegistry) -- the AS export-name class-registry hash.
    // Guarded (the console null-checks it): destroy its held GC pointers, then run its
    // scalar-deleting destructor -- dtor + pool free back to the non-GC DOGMA pool it
    // was placement-new'd from (AptObject.cpp) -- then null the slot.
    if (gpAptClassRegistry != nullptr)
    {
        gpAptClassRegistry->DestroyGCPointers();
        if (gpAptClassRegistry != nullptr)
        {
            gpAptClassRegistry->~AptNativeHash();
            if (gpNonGCPoolManager != nullptr)
                gpNonGCPoolManager->Deallocate(gpAptClassRegistry, sizeof(AptNativeHash));
        }
        gpAptClassRegistry = nullptr;
    }

    // FLAG (PC): first interrupt-masked TAS on unk_8324E71C elided single-threaded.

    // off_8324E37C -- the _global extension object.
    gpAptGlobalExtensionObject->DestroyGCPointers();   // vtbl +0x28
    gpAptGlobalExtensionObject->Release();             // vtbl +0x04 (drops the boot AddRef)
    gpAptGlobalExtensionObject = nullptr;

    // FLAG (PC): second interrupt-masked TAS on unk_8324E71C elided single-threaded.

    // off_8324E380 -- the _global fallback scope.
    gpAptGlobalFallback->DestroyGCPointers();
    gpAptGlobalFallback->Release();
    gpAptGlobalFallback = nullptr;

    // off_8324E2A8 -- the Key manager singleton.
    gpAptKeyObject->DestroyGCPointers();
    gpAptKeyObject->Release();
    gpAptKeyObject = nullptr;

    // off_8324D82C -- the shared empty AptString.
    gpAptStringObject->DestroyGCPointers();
    gpAptStringObject->Release();
    gpAptStringObject = nullptr;

    // dword_8324E2AC -- the shared AptRenderingContext (a pooled non-AptValue); guarded
    // scalar-deleting destructor (dtor + pool free). The slot is NOT nulled (asm).
    if (gpAptRenderingContext != nullptr)
        static_cast<AptRenderingContext*>(gpAptRenderingContext)->ScalarDeletingDestructor(1);

    // The seven AS global native-function singletons (deferred-null; see the FLAG).
    gpAptNativeFn_setInterval->DestroyGCPointers();
    gpAptNativeFn_setInterval->Release();
    gpAptNativeFn_setInterval = nullptr;

    gpAptNativeFn_clearInterval->DestroyGCPointers();
    gpAptNativeFn_clearInterval->Release();
    gpAptNativeFn_clearInterval = nullptr;

    gpAptNativeFn_isNaN->DestroyGCPointers();
    gpAptNativeFn_isNaN->Release();
    gpAptNativeFn_isNaN = nullptr;

    gpAptNativeFn_unescape->DestroyGCPointers();
    gpAptNativeFn_unescape->Release();
    gpAptNativeFn_unescape = nullptr;

    gpAptNativeFn_escape->DestroyGCPointers();
    gpAptNativeFn_escape->Release();
    gpAptNativeFn_escape = nullptr;

    gpAptNativeFn_boolean->DestroyGCPointers();
    gpAptNativeFn_boolean->Release();
    gpAptNativeFn_boolean = nullptr;

    gpAptNativeFn_ASSetPropFlags->DestroyGCPointers();
    gpAptNativeFn_ASSetPropFlags->Release();
    gpAptNativeFn_ASSetPropFlags = nullptr;

    // off_8324D748 -- the Object.registerClass native. The console tail-returns this
    // last Release's r3; AptValue::Release is void here, so 0 (the vendor-path result).
    gpObjRegistrationFunc->DestroyGCPointers();
    gpObjRegistrationFunc->Release();
    gpObjRegistrationFunc = nullptr;
    return 0;
}

// ---------------------------------------------------------------------------
// Un-homed siblings/globals the shutdown paths (AptCommonShutdown +
// AptUpdateShutdown) reach; declared here at the call site (the codebase pattern).
// ---------------------------------------------------------------------------
// The per-thread current-target TLS mirror (unk_8324E814); defined in
// AptRenderLinkStubs.cpp. (Re-declared later in the AptUpdateTarget section; a
// redundant extern is harmless and keeps the shutdown bodies self-contained.)
extern EA::Thread::ThreadLocalStorage gAptTargetTls;   // unk_8324E814

// byte_8324E7C9 -- the "in a shutdown / on the owning thread" guard AptTarget.cpp owns;
// AptUpdateShutdown forces it to 1 before the animation-director predestroy. Defined in
// AptGlobals.cpp; declared extern here.
extern unsigned char gbAptInShutdown;   // byte_8324E7C9

// AptUpdateZombieVector @0x82B0.. (homed in AptGC.cpp) -- reap the per-target zombie
// vector, retiring the entries flagged this pass. Canonical void* return.
extern void* AptUpdateZombieVector(char bClear);

// The AptAnimationTarget lifecycle shims (homed in AptAnimationTarget.cpp; the same
// free wrappers AptTarget::Shutdown uses): PreDestroy releases the director's owned
// state, CleanRemList drains the shared delayed-release list (a static op -- the pAnim
// arg is accepted for call parity, matching the console's r3).
// (The AptAnimationTarget_PreDestroy / _CleanRemList free-function shims are RETIRED --
// AptTarget.cpp calls the real members directly; the shutdown below does the same.)

// ===========================================================================
// AptCommonShutdown @0x82B0AC08 -- the once-only shared static-data teardown
// (counterpart of AptCommonInitialize @0x82AE91F0). Records the sim/render thread ids,
// drops the render latch, tears down the animation-target static data + the active
// target instance, clears the three context globals + the TLS mirror, runs the AS
// value-singleton teardown (AptValueShutdownRemaining), resets the config-default string
// + tears down the string pool, drains + frees the shared deferred-release vector, clears
// the integer/float free pools + the temporary string pool, then clears the common-init
// self-registration slot + the once-only init guard.
//
// FLAG (PC): AptSetRenderThreadID/AptSetSimulationThreadID take an int the vendor
// GetThreadId ignores (the console threads r3 through them -- dead plumbing); the two
// interrupt-masked TAS spin locks (reached via the sub-calls, and the final unk_8324E6E0
// latch clear) are single-threaded no-ops (the AptInit.cpp spin-lock treatment). The
// return is StringPool::ClearTemporaryPool's r3, void here -> 0.
// ===========================================================================
int AptCommonShutdown()
{
    // Record the sim/render thread ids (the console threads r3 through both; the arg is
    // unused on the vendor GetThreadId -- see AptSetRenderThreadID's FLAG).
    AptSetRenderThreadID(0);
    AptSetSimulationThreadID(0);

    gbAptRenderInitDone = 0;   // dword_8324E510 = 0

    AptAnimationTarget::CleanupStaticData();

    // Tear down + free the active target instance (list head off_8324E570).
    AptTarget* lpCurrent = gpAptTargetCurrent;   // v2 = off_8324E570
    if (lpCurrent != nullptr)
    {
        lpCurrent->Shutdown();                                          // AptTarget::Shutdown
        gpAptPseudoDataPool->Deallocate(lpCurrent, sizeof(AptTarget));  // Deallocate(pool, v2, 48)
    }

    // Clear the three context globals + the per-thread TLS mirror.
    gpAptTarget        = nullptr;      // off_8324E574 = 0
    gpAptTargetCurrent = nullptr;      // off_8324E570 = 0
    gpAptTargetTLS     = nullptr;      // off_8324E578 = 0
    gAptTargetTls.SetValue(nullptr);   // TLS(unk_8324E814) = 0

    // The AS value-singleton teardown (undefined / boolean / lookup / register /
    // character-helper / extern-interface).
    AptValueShutdownRemaining();

    // off_82F733B0 (gpAptConfigDefault): the console releases the config-default EAStringC's
    // buffer (EAStringC::DecreaseInternalRefCount) then resets its m_pData to the shared
    // empty StringDataC (unk_82F72FF8). This reconstruction models gpAptConfigDefault as the
    // opaque sentinel pointer AptUpdateInitialize only ever points at gAptEmptyDefaultSentinel
    // (the shared empty StringDataC, whose refcount is pinned), so the release is inert; the
    // reset to the sentinel is the faithful visible effect. (DecreaseInternalRefCount +
    // StringDataC are EAStringC-private, so the release is not callable here; the modeled
    // pinned sentinel makes it a no-op, avoiding an EAStringC befriend.)
    gpAptConfigDefault = &gAptEmptyDefaultSentinel[0];   // off_82F733B0 = &unk_82F72FF8
    AptStringPool_Teardown();                            // StringPool::Teardown

    // Drain + free the shared common-init deferred-release vector (off_8324E51C). The console
    // calls ReleaseValues unconditionally (the pointer is non-null on the live init path),
    // then frees the item array (4*capacity) + the 12-byte header when present.
    gpAptDeferredVecCommon->ReleaseValues();          // AptValueVector::ReleaseValues(off_8324E51C)
    AptValueVector* lpVec = gpAptDeferredVecCommon;   // v5 = off_8324E51C
    if (lpVec != nullptr)
    {
        // Family-(B) vector (AptValueVector.h): the +0 word (mnTop member) holds the
        // capacity; the item array is 4*capacity, the header is the 12-byte block.
        gpAptPseudoDataPool->Deallocate(lpVec->mppItems, sizeof(AptValue*) * lpVec->mnTop);  // 4 * *v5
        gpAptPseudoDataPool->Deallocate(lpVec, sizeof(AptValueVector));                       // 12
    }
    gpAptDeferredVecCommon = nullptr;   // off_8324E51C = 0

    // Clear the integer/float free pools (befriended) + the temporary string pool. The
    // console threads r3 through the two ClearPool calls (both void) into ClearTemporaryPool.
    AptInteger::ClearPool();
    AptFloat::ClearPool();
    AptStringPool_ClearTemporaryPool();   // StringPool::ClearTemporaryPool (its r3 -> result)

    gpAptCommonFuncsSlot = nullptr;   // dword_8324E4F0 = 0
    // FLAG (PC): the final interrupt-masked TAS clearing the once-only init guard
    // (dword_8324E6E0) is a single-threaded no-op; the guard store is the visible reset.
    gbAptCommonInitDone = 0;          // dword_8324E6E0 = 0

    return 0;   // console r3 == StringPool::ClearTemporaryPool result (void here -> 0)
}

// ===========================================================================
// AptRenderInitialize @0x82AEDE90 -- bring up the Apt render side.
//
// X360: Mutex::Lock(unk_8324E7E0, unk_82143270); if (!dword_8324E6E0)
// AptCommonInitialize(&unk_82F733B8); v0 = AptMath::ClipStackInit(128);
// dword_8324E510 = 1; AptSetRenderThreadID(v0); Mutex::Unlock(unk_8324E7E0);
// off_8324E2C8 = DOGMA_PoolManager::Allocate(off_8324D808, 4 * dword_82F73004).
//
// dword_82F73004 == 1024 (read-only .data constant, verified from the XEX image).
// ClipStackInit returns the top clip-entry pointer; AptSetRenderThreadID's arg is
// that pointer (the console passes r3 straight through; on the vendor GetThreadId()
// the arg is unused -> the current-thread id is recorded either way).
// ===========================================================================
static const unsigned int gnAptRenderItemPoolCount = 1024u;   // dword_82F73004

void* AptRenderInitialize(int /*a1*/)
{
    AptUpdateRenderMutex().Lock();

    if (!gbAptCommonInitDone)
        AptCommonInitialize(&gAptCommonConfig[0]);

    intptr_t liTop = AptMath::ClipStackInit(128);    // x64: pointer-width (see AptMath FLAG)
    gbAptRenderInitDone = 1;                          // dword_8324E510 = 1
    AptSetRenderThreadID(static_cast<int>(liTop));    // console passes the clip-top ptr; the arg is unused

    AptUpdateRenderMutex().Unlock();

    // off_8324E2C8 = pool->Allocate(4 * 1024) -- the render-item pointer pool.
    gpAptRenderItemPool = gpAptPseudoDataPool->Allocate(sizeof(void*) * gnAptRenderItemPoolCount);
    return gpAptRenderItemPool;
}

// ===========================================================================
// AptRenderShutdown @0x82B0C2F0 -- tear down the Apt render side (counterpart of
// AptRenderInitialize): clear the render latch, free the clip stack, run the
// common teardown when the sim side is already down, then drain + free the
// render-item pointer pool.
//
// X360: dword_8324E510 = 0; v0 = AptMath::ClipStackShutdown(); if (!dword_8324E50C)
// AptCommonShutdown(v0); if (dword_8324E508) { Mutex::Lock(unk_8324E728);
// for (i.. dword_8324E508) { dword_8324E870(pool[i]); pool[i] = 0; }
// dword_8324E508 = 0; Mutex::Unlock(unk_8324E728); } result =
// DOGMA_PoolManager::Deallocate(off_8324D808, off_8324E2C8, 4 * dword_82F73004);
// off_8324E2C8 = 0; dword_8324E508 = 0.
//
// FLAG (x64): the pool holds pointer-width slots (AptRenderInitialize allocated
// sizeof(void*) * count), so the drain strides by void* and the Deallocate size
// mirrors that Allocate size -- the console's literal `4 * count` byte math is the
// 32-bit-pointer form of the same walk.
// ===========================================================================
int AptRenderShutdown()
{
    gbAptRenderInitDone = 0;                 // dword_8324E510 = 0

    AptMath::ClipStackShutdown();            // free the render clip stack (X360 r3 -> v0)
    if (!gbAptUpdateInitDone)                // dword_8324E50C: skip the common teardown while the sim side is up
        AptCommonShutdown();                 // (X360 carries the ClipStackShutdown result in r3; recovered sig takes none)

    if (gAptDeferredReleaseCount)            // dword_8324E508
    {
        AptUnresolveMutex().Lock();

        unsigned int liFreed = 0;            // v1
        if (gAptDeferredReleaseCount)        // re-read under the lock
        {
            void** const lppItems = static_cast<void**>(gpAptRenderItemPool);   // off_8324E2C8
            do
            {
                AptFreeFontUnit(lppItems[liFreed]);   // dword_8324E870(*(off_8324E2C8 + v2))
                lppItems[liFreed] = nullptr;          // *(off_8324E2C8 + v2) = 0
                ++liFreed;
            }
            while (liFreed < static_cast<unsigned int>(gAptDeferredReleaseCount));
        }

        gAptDeferredReleaseCount = 0;        // dword_8324E508 = 0
        AptUnresolveMutex().Unlock();
    }

    // off_8324E2C8 = pool->Deallocate(4 * 1024) -- release the render-item pointer pool.
    bool lbResult = gpAptPseudoDataPool->Deallocate(gpAptRenderItemPool,
                                                    sizeof(void*) * gnAptRenderItemPoolCount);
    gpAptRenderItemPool      = nullptr;      // off_8324E2C8 = 0
    gAptDeferredReleaseCount = 0;            // dword_8324E508 = 0
    return lbResult;
}

// ===========================================================================
// AptUpdateInitialize @0x82B02D08 -- bring up the Apt sim/update side.
//
// X360: Mutex::Lock; AptSetSimulationThreadID(r3); build the default AptUpdateParams
// (v13[]) when a1 is null; memcpy(&unk_82F733B8, params, 68); if (!off_82F733B0)
// off_82F733B0 = &unk_82F72FF8; byte_8324E392 = 0; byte_8324E393 = params[+0x3D];
// if (!dword_8324E820) dword_8324E820 = sub_82AD9030; if (!dword_8324E6E0)
// AptCommonInitialize(&unk_82F733B8); if (params[14] && (m=Allocate(12)))
// off_8324E528 = AptValueVector(m, params[14]) else off_8324E528 = 0;
// AptValueInitialize(); AptActionInterpreter::initialize(&dword_8324E760, params);
// build the 28-byte single-list node (dword_8324D810); if (a2) create+select the
// initial AptTarget; dword_8324E50C = 1; Mutex::Unlock.
// ===========================================================================

// sub_82AD9030 -- the DOGMA sized-free hook (DOGMA_FreeSized), via the adapter above.

// The zombie vector (off_8324E528) -- defined in AptGC.cpp next to its reap;
// allocated here from config word [14] (default 8).
extern AptValueVector* gpAptZombieVector;

int AptUpdateInitialize(unsigned int* a1, char a2)
{
    AptUpdateRenderMutex().Lock();

    AptSetSimulationThreadID(0);   // console r3 == the sim-thread-id arg (0 == current)

    // The built-in default AptUpdateParams (v13[15]) the console uses when a1 is null.
    // Transcribed from the asm store set (0x82B02D38..0x82B02DB0):
    //   [0]512 [1]512 [2]64 [3]256 [4]64 [5]256 [6]384 [7]64 [8]384 [9]32 [10]256
    //   [11]1024 [12]128 [13]128 [14]8  + trailing bytes {0,0,0,0,0}.
    unsigned int lauDefault[17] = {
        512u, 512u, 64u, 256u, 64u, 256u, 384u, 64u,
        384u, 32u, 256u, 1024u, 128u, 128u, 8u, 0u, 0u
    };
    unsigned int* lpParams = a1 ? a1 : lauDefault;

    // memcpy(&unk_82F733B8, params, 68).
    std::memcpy(&gAptCommonConfig[0], lpParams, 68);

    // dword_82F733E8 (config +0x30, word 12) -- the AS register count. The console
    // reads the config slot in place (AptRegister::Initialize / InitializeStaticData
    // both read +0x30); our reconstruction carries it as the gnAptRegisterCount
    // global, published here so those readers see the configured value (default 128).
    gnAptRegisterCount = static_cast<s32>(lpParams[12]);

    // if (!off_82F733B0) off_82F733B0 = &unk_82F72FF8.
    if (gpAptConfigDefault == nullptr)
        gpAptConfigDefault = &gAptEmptyDefaultSentinel[0];

    // byte_8324E392 = 0; byte_8324E393 = params byte[0x3D].
    gAptDefaultTextMouseWheelEnabled = false;
    gbAptUpdateFlag393 = reinterpret_cast<const unsigned char*>(lpParams)[0x3D];

    // if (!dword_8324E820) dword_8324E820 = sub_82AD9030 (the DOGMA sized-free hook).
    if (gpAptGCTableFree == nullptr)
        gpAptGCTableFree = &AptDogmaFreeSizedHook;

    // if (!dword_8324E6E0) AptCommonInitialize(&unk_82F733B8).
    if (!gbAptCommonInitDone)
        AptCommonInitialize(&gAptCommonConfig[0]);

    // if (params[14] && (m = Allocate(12))) off_8324E528 = AptValueVector(m, params[14]).
    if (lpParams[14] != 0)
    {
        void* lpVecMem = gpAptPseudoDataPool->Allocate(sizeof(AptValueVector));   // console 12; x64 sizeof
        if (lpVecMem != nullptr)
        {
            AptValueVector* lpVec = static_cast<AptValueVector*>(lpVecMem);
            *lpVec = AptValueVector::ConstructAptValueVector(static_cast<int>(lpParams[14]));
            gpAptZombieVector = lpVec;
        }
        else
        {
            gpAptZombieVector = nullptr;
        }
    }
    else
    {
        gpAptZombieVector = nullptr;
    }

    // AptValueInitialize() -- FLAG-deferred (protected value ctors; see its body).
    AptValueInitialize();

    // Fill the opcode->handler dispatch table before the interpreter comes up. The
    // console ships sGlobalTable as pre-initialised static DATA; the extracted map is
    // filled at the same bring-up point here (equivalent result; unbuilt opcodes get
    // the no-op stub).
    AptActionInterpreter::InitDispatchTable();

    // AptActionInterpreter::initialize(&dword_8324E760, params). The homed initialize
    // takes a const AptInitParmsT* and reads the config block at +0x20/+0x24/+0x30/
    // +0x40 -- the SAME layout as the AptUpdateParams (iStackSize == word[8],
    // iCallStackDepth == word[9], iRegisterCount == word[12], skip-trace == byte[64]).
    gAptActionInterpreter.initialize(reinterpret_cast<const AptInitParmsT*>(lpParams));

    // Build the 28-byte single-list free node (dword_8324D810). The console pool-
    // allocates 28 bytes then seeds an intrusive 2-element free list (head/tail null,
    // body self-referential, entries pointing at &unk_82F72FF8). This node is engine-
    // internal scratch, never serialised; the byte-faithful zeroed + sentinel-seeded
    // 28-byte block preserves the initialised state (member roles TBD -> // FLAG).
    void* lpNode = gpAptPseudoDataPool->Allocate(28);
    if (lpNode != nullptr)
    {
        std::memset(lpNode, 0, 28);
        gpAptUpdateListNode = lpNode;
    }
    else
    {
        gpAptUpdateListNode = nullptr;
    }

    // if (a2) create + select the initial AptTarget context; else null the three
    // context globals. NB: InitializeApt calls AptUpdateInitialize with a2 == 0 (it
    // creates the context separately via AptCreateTargetInstance), so this branch is
    // normally NOT taken on the boot path -- kept faithful for completeness.
    if (a2)
    {
        void* lpTgtMem = gpAptPseudoDataPool->Allocate(sizeof(AptTarget));
        AptTarget* lpTgt = lpTgtMem ? new (lpTgtMem) AptTarget(lpParams) : nullptr;
        gpAptTargetCurrent = lpTgt;   // off_8324E570 (list head)
        // off_8324E574/578 + the TLS mirror (unk_8324E814) via AptChangeTargetInstance
        // (which AptTarget.cpp owns) -- it sets gpAptTarget/gpAptTargetTLS + the TLS
        // slot GetTarget() reads. (Setting gpAptTargetCurrent above covers the list head
        // AptChangeTargetInstance does not touch.)
        if (lpTgt != nullptr)
            AptChangeTargetInstance(lpTgt);
        else
        {
            gpAptTarget    = nullptr;
            gpAptTargetTLS = nullptr;
        }
    }
    else
    {
        gpAptTargetCurrent = nullptr;
        gpAptTarget        = nullptr;
        gpAptTargetTLS     = nullptr;
    }

    gbAptUpdateInitDone = 1;   // dword_8324E50C = 1

    AptUpdateRenderMutex().Unlock();
    return 0;   // the console returns the Mutex::Unlock result; 0 on the vendor path
}

// ===========================================================================
// AptActionInterpreter::shutdown @0x82AE15A0 -- the folded interpreter teardown (see
// the additive AptActionInterpreter.h declaration). The X360 linker COMDAT-folded it
// into AptValueVector::shutdown @0x82AE15A0 (byte-identical body); the interpreter's
// leading operand-stack vector (mnStackTop/mnStackCapacity/mpStack == the AptValueVector
// {mnTop,mnCapacity,mppItems} at +0/+4/+8) IS that vector, so the call frees the
// operand-stack array and zeros the three fields, in the console's store order (items
// free, capacity, top, items). Reconstructed store-for-store from AptValueVector::
// shutdown (AptValueVector.cpp @0x82AE15A0), on the interpreter's OWN members by name
// (no offset cast). Homed here beside its sole caller, AptUpdateShutdown.
// ===========================================================================
void AptActionInterpreter::shutdown()
{
    if (mpStack != nullptr)
        gpAptOperandStackPool->Deallocate(mpStack, sizeof(AptValue*) * mnStackCapacity);
    mnStackCapacity = 0;
    mnStackTop      = 0;
    mpStack         = nullptr;
}

// ===========================================================================
// AptUpdateShutdown @0x82B0C170 -- tear down the Apt sim/update side (counterpart of
// AptUpdateInitialize): force the in-shutdown guard + drop the update latch, clean each
// value type's AS native-function table, predestroy the active target's animation
// director (as the current context), run the AS value-singleton teardown
// (AptValueShutdown), reap the zombie vector, free the saved-input-checkpoint vector
// node, run the GC's full clean, drain the deferred-release + zombie vectors, shut the
// interpreter down, and -- when the render side is already down -- run the shared common
// teardown.
//
// FLAG (PC): the console brackets nothing here with a lock. `a1` is AptValueShutdown's
// unused arg (threaded r3). The return is AptCommonShutdown's result, or the interpreter-
// shutdown r3 leftover (void) when the common teardown is skipped -> 0.
// ===========================================================================
int AptUpdateShutdown(int /*a1*/)
{
    gbAptInShutdown     = 1;   // byte_8324E7C9 = 1
    gbAptUpdateInitDone = 0;   // dword_8324E50C = 0

    // Clean each value type's lazily-built AS native-function table (the console threads
    // r3 through the chain; each is a static void teardown).
    AptArray::CleanNativeFunctions();
    AptKey::CleanNativeFunctions();
    AptScriptColour::CleanNativeFunctions();
    AptCIH::CleanNativeFunctions();
    AptString::CleanNativeFunctions();
    AptError::CleanNativeFunctions();

    // Predestroy the active target's animation director, running "as" that context. The
    // console saves/restores the current + TLS-mirror context pointers around the pass
    // (a redundant dance -- both saved values equal the current instance -- reproduced
    // store-for-store).
    AptTarget* lpCurrent = gpAptTargetCurrent;   // r4 = off_8324E570
    gpAptTarget    = lpCurrent;                   // off_8324E574 = off_8324E570
    gpAptTargetTLS = lpCurrent;                   // off_8324E578 = off_8324E570
    if (lpCurrent != nullptr)
    {
        gpAptTarget = lpCurrent;                       // off_8324E574 (console re-store)
        AptTarget* lpPrevTarget = lpCurrent;           // r27 -- gpAptTarget restore value
        gAptTargetTls.SetValue(lpCurrent);
        AptTarget* lpPrevTLS = gpAptTargetTLS;         // r26 -- gpAptTargetTLS restore value (== lpCurrent)
        gpAptTargetTLS = lpCurrent;                    // off_8324E578 = off_8324E570
        gAptTargetTls.SetValue(lpCurrent);

        AptAnimationTarget* lpAnim = lpCurrent->mpAnimationTarget;   // *(off_8324E570 + 0x18)
        lpAnim->PreDestroy();                          // release the director's owned state
        AptAnimationTarget::CleanRemList();            // drain the shared delayed-release list

        gpAptTargetTLS = lpPrevTLS;                    // off_8324E578 = r26 (restore)
        gAptTargetTls.SetValue(lpPrevTLS);
        gpAptTarget = lpPrevTarget;                    // off_8324E574 = r27 (restore)
        gAptTargetTls.SetValue(lpPrevTarget);
    }

    // The AS value-singleton teardown (mirror of AptValueInitialize).
    AptValueShutdown();

    // Reap every zombie this pass (the r3 result is the dead leftover carried into
    // AptGC::CleanAll below).
    AptUpdateZombieVector(1);

    // Free the saved-input-checkpoint vector node (dword_8324D810). StringAsVec @0x82AF83F8
    // is the AptFileSavedInputStateVector destructor (dword_8324D810 == the
    // gpAptSavedInputCheckpoints vector; see AptSavedInputCheckpoints.cpp); it frees the
    // vector's heap element backing. On the opaque, zeroed 28-byte node AptUpdateInitialize
    // models (count 0 / SBO -- its member layout deliberately un-reconstructed), there is no
    // separately-allocated backing to free, so the teardown reduces to the 28-byte block
    // Deallocate below. // FLAG
    void* lpNode = gpAptUpdateListNode;   // v10 = dword_8324D810
    if (lpNode != nullptr)
    {
        gpAptPseudoDataPool->Deallocate(lpNode, 28);   // Deallocate(pool, v10, 28)
    }
    gpAptUpdateListNode = nullptr;   // dword_8324D810 = 0

    // The GC's full value-pool clean (the console r3 into it is the dead reap/Deallocate
    // leftover; CleanAll takes no args).
    AptGC::CleanAll();

    // Drain the shared common-init deferred-release vector (off_8324E51C), unconditional
    // (non-null on the live path).
    gpAptDeferredVecCommon->ReleaseValues();   // AptValueVector::ReleaseValues(off_8324E51C)

    // Free the per-target zombie vector (off_8324E528): item array (4*capacity) + 12-byte
    // header (family-(B) vector -- the +0 word / mnTop member holds the capacity).
    AptValueVector* lpZombie = gpAptZombieVector;   // v11 = off_8324E528
    if (lpZombie != nullptr)
    {
        gpAptPseudoDataPool->Deallocate(lpZombie->mppItems, sizeof(AptValue*) * lpZombie->mnTop);  // 4 * *v11
        gpAptPseudoDataPool->Deallocate(lpZombie, sizeof(AptValueVector));                          // 12
    }
    gpAptZombieVector = nullptr;   // off_8324E528 = 0

    // Shut the interpreter (operand stack) down; the console keeps its r3 (void -> 0).
    gAptActionInterpreter.shutdown();   // AptActionInterpreter::shutdown(&dword_8324E760)
    int liResult = 0;

    // When the render side is already down, run the shared common teardown too.
    if (!gbAptRenderInitDone)   // dword_8324E510
        liResult = AptCommonShutdown();

    return liResult;
}

// ===========================================================================
// AptUpdateTarget @0x82B0DE80 -- tick ONE Apt target's movies as the current
// context, then restore the previous context.
//
// X360: v5 = off_8324E574 (save gpAptTarget); off_8324E574 = a1; TLS(unk_8324E814)
// = a1; AptUpdate(a2, a3, a4); off_8324E574 = v5; result = TLS(unk_8324E814) = v5.
// i.e. make pTarget the current context (the global slot + the per-thread TLS
// mirror GetTarget() reads), run the per-frame AptUpdate against it, then restore
// the previous context. Returns the EA TLS SetValue result (X360 r3).
//
// NB: only gpAptTarget (off_8324E574) + the TLS mirror are swapped here -- NOT the
// list head (off_8324E570) nor gpAptTargetTLS (off_8324E578); the asm touches only
// those two slots (unlike AptChangeTargetInstance, which sets ..E574 + ..E578). The
// console brackets nothing with a lock on this path.
//
// AptUpdate is the per-frame Apt sim tick, homed by its own (pending) TU; its
// recovered (a2,a3,a4) int args are forwarded verbatim. Declared extern here.
// ===========================================================================

// The per-thread current-target TLS mirror (unk_8324E814). Defined in
// AptRenderLinkStubs.cpp; AptTarget.cpp publishes into the same slot.
extern EA::Thread::ThreadLocalStorage gAptTargetTls;

// AptUpdateTarget @0x82B0DE80 (the context swap around the per-frame AptUpdate
// @0x82B0DB68) is homed in AptUpdate.cpp with the frame pacer; the duplicate body
// that briefly lived here is retired at the l2 merge.
