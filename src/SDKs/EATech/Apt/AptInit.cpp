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
//   * StringPool::Initialize points the interned AS-name table at the 264-byte
//     StaticStringHelperT record block sStringPoolData @0x82F733FC -- contents
//     RECOVERED (the 0x82F733FC dump confirms record 0 == {refCount 1, length 9,
//     flags 1, 0} + "__proto__"; the CRT initializer sub_82C71F10 fills records
//     1..87 at runtime) and statically interned at the saConstant definition
//     (AptStringPool.cpp); the structural pool-array allocation stays faithful.
//   * AptValueInitialize @0x82B02800 (the AS value-singleton bootstrap) is homed
//     here IN FULL (befriended into the value classes): the value singletons, the
//     nine-entry AS builtin class table + prototype seeding (the console's
//     tail-call sub_82AF6B68), and the seven AS global native-function singletons
//     over their real cbCallMethod_* bodies (AptIntervalTimer.cpp /
//     AptActionInterpreterBuiltins.cpp).
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

// ---- the saved-input checkpoint list (dword_8324D810; constructed here) -----
#include "SDKs/EATech/include/Apt/AptSavedInputCheckpoints.h"   // AptFileSavedInputStateVector + gpAptSavedInputCheckpoints

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

    // (the deferred-release vector: the ONE off_8324E51C global gpValuesToRelease
    //  lives in AptGlobals.cpp -- declared below, OUTSIDE this anonymous namespace.)

    // (the saved-input checkpoint list dword_8324D810 == gpAptSavedInputCheckpoints
    //  is owned by AptSavedInputCheckpoints.cpp; AptUpdateInitialize constructs it.)

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
    // Vendor EA::Thread::Mutex (default recursive). FLAG PC-platform leaf: the
    // console passes the raw MutexParameters name (unk_82143270); a default-
    // constructed recursive vendor mutex is the same lock (threading primitive,
    // not an engine method). Function-local static -> init-order safe.
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
    // AptUpdateRenderMutex (init-order safe). FLAG PC-platform leaf: the console
    // passes the raw MutexParameters name (unk_82143270); a default recursive
    // vendor mutex is the same lock (threading primitive, not an engine method).
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

// The ONE deferred-release vector pointer (off_8324E51C; console symbol
// gValuesToRelease). Defined in AptGlobals.cpp; allocated + family-(B)-constructed
// by AptCommonInitialize below, nulled by AptCommonShutdown.
extern AptValueVector* gpValuesToRelease;   // off_8324E51C

// The DOGMA sized-free hook (dword_8324E820) the update-init installs when null.
extern void (*gpAptGCTableFree)(void* p, unsigned nBytes);   // dword_8324E820 (AptGlobals.cpp)

// byte_8324E392 -- the mouse-wheel default flag (AptGlobals.cpp).
extern bool gAptDefaultTextMouseWheelEnabled;   // byte_8324E392

// dword_8324E504 -- the render-thread id (AptGlobals.cpp; AptTarget.cpp reads it).
extern void* gAptRenderThreadId;   // dword_8324E504 (EA::Thread::ThreadId == void*)

// The interpreter VM singleton (dword_8324E760). Defined in AptGlobals.cpp.
extern AptActionInterpreter gAptActionInterpreter;

// gAptFuncs (dword_8324E818) -- the host user-function table, defined in CgsAptAux.cpp.
// The full layout comes from Apt.h: AptAllocatorInitialize calls its FIRST member
// (pfnMemAlloc == the console's dword_8324E818(48) base-alloc), so the complete
// type is needed here.
#include "SDKs/EATech/include/Apt/Apt.h"
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
// AptMath::ClipStackShutdown @0x82AE24E8 -- free the render clip stack (the
// ClipStackInit inverse). HOMED in AptMath.cpp (targeted export 2026-08-07);
// declared at the call site because AptMath.h's interpreter-adjacent include
// set stays out of this TU.
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
// The pool-manager OBJECTS come from the host base-alloc hook: the console's
// dword_8324E818(48) is gAptFuncs.pfnMemAlloc (CgsGui::AptCallbackMemory::Alloc
// @0x828491C8, installed by AptAux::ConstructApt before InitializeApt chains here);
// console 48 covers the 32-bit object, x64 allocates sizeof. Neither object is ever
// freed -- process-lifetime, matching the console. The GC min/max statics come
// straight from StaticInitialize's scan of the per-VFT size table (byte_82144A18
// rodata RECOVERED -- 0x82144A18 dump: the console-shipped scan yields min 0x00 /
// max 0x44; StaticInitialize regenerates the x64 table contents from the
// reconstructed class set before scanning -- AptValueGCPoolManager.cpp), so the
// old x64 4/256 override FLAG is retired. WireAllocatorGlobals
// sets off_8324D808/off_8324D834 (+ the operand/pseudo/render/shared/single-list
// aliases the engine reads off the non-GC pool). The DOGMA fixed-size params
// (minSize 4, maxSize 256, 0/0/0, bTrackOutsideAllocations 1) are transcribed from
// the @0x82ADD118 asm.
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

    // mem = dword_8324E818(48); off_8324D808 = DOGMA_PoolManager(mem, ...).
    void* lpDogmaMem = gAptFuncs.pfnMemAlloc(sizeof(DOGMA_PoolManager));   // console 48; x64 sizeof
    s_pDogmaPool = lpDogmaMem
                       ? ::new (lpDogmaMem) DOGMA_PoolManager(nDogmaMain, nDogmaOvf,
                                                            /*minSize*/ 4, /*maxSize*/ 256,
                                                            /*nOffsetToStoreNextInFreeItem*/ 0,
                                                            /*bStoreFreeBlockSize*/ false,
                                                            /*nOffsetToStoreSizeInFreeItem*/ 0,
                                                            /*bTrackOutsideAllocations*/ true)
                       : nullptr;

    // mem2 = dword_8324E818(48); off_8324D834 = AptValueGC_PoolManager(mem2, gcMain, gcOvf).
    void* lpGCMem = gAptFuncs.pfnMemAlloc(sizeof(AptValueGC_PoolManager));   // console 48; x64 sizeof
    s_pGCPool = lpGCMem ? ::new (lpGCMem) AptValueGC_PoolManager(nGcMain, nGcOvf) : nullptr;

    WireAllocatorGlobals();
    return s_pGCPool;
}

// ===========================================================================
// AptSetSimulationThreadID @0x82AD8F90 / AptSetRenderThreadID @0x82AD8EF0
//
// X360: interrupt-masked TAS on unk_8324E724; id = EA::Thread::GetThreadId(a1);
// store into the sim (dword_8324E500) / render (dword_8324E504) slot; unlock.
//
// FLAG PC-platform leaf: thread-id plumbing -- the console's GetThreadId(a1) takes
// a thread-handle arg; the vendor EA::Thread::GetThreadId() is the no-arg "current
// thread" form (the value the console yields here -- these are called on the main
// thread with a1 that resolves to the caller). `a1` is accepted for signature
// parity but unused (the vendor has no by-handle GetThreadId on this build).
// ===========================================================================
int AptSetSimulationThreadID(int /*a1*/)
{
    AptThreadIdLock_Acquire();                 // FLAG PC-platform leaf: interrupt-masked TAS elided (single-threaded)
    EA::Thread::ThreadId id = EA::Thread::GetThreadId();
    gAptSimThreadId = id;                       // dword_8324E500 = result
    AptThreadIdLock_Release();
    return static_cast<int>(reinterpret_cast<intptr_t>(id));
}

int AptSetRenderThreadID(int /*a1*/)
{
    AptThreadIdLock_Acquire();                 // FLAG PC-platform leaf: interrupt-masked TAS elided (single-threaded)
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

    // FLAG PC-platform leaf: the console's interrupt-masked TAS on dword_8324E6E0 is
    // elided (single-threaded). Setting the guard (=1) is the faithful visible result.
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
        gpValuesToRelease = lpVec;
        return lpVec;
    }
    gpValuesToRelease = nullptr;
    return nullptr;
}

// ===========================================================================
// AptValueInitialize @0x82B02800 -- the AS value-singleton bootstrap (wired IN FULL).
//
// The console builds the AS value singletons here and drains the common-init
// deferred-release vector. In the console's order:
//   1. off_8324D814 (gpUndefinedValue) = a pooled AptNone -- the shared `undefined`
//      (X360: Allocate(off_8324D808, 8) then AptNone::AptNone(); the pooled operator
//      new + the befriended protected ctor reproduce that on x64 widths).
//   2. AptBoolean::Initialize()  (the shared true/false pair; befriended)
//   3. AptLookup::Initialize()   (the slot-indexed lookup table; reads
//      gAptLookupPoolSize, published from the config block by AptUpdateInitialize)
//   4. AptRegister::Initialize() (the AS register-value file; reads gnAptRegisterCount,
//      published the same way), then off_8324E498/E49C = 0 (the cached default
//      text/movie character templates reset)
//   5. gpAptEmptyCIH (dword_8324D700) = a pinned AptCIHNone named "EmptyCIH" (the
//      absent-CIH placeholder handle; befriended)
//   6. AptRenderingContext, AptExtern, AptKey (+AddRef), AptGlobal (+AddRef), the
//      _global extension object (+AddRef), the shared empty AptString (+root+AddRef)
//   7. dword_8324E2D4 = 0, then the AS-globals bootstrap (the console's tail-call
//      sub_82AF6B68): nine builtin class natives + prototypes + Object.registerClass
//   8. the colour-transform/matrix default stores (const-subsumed, see below) and
//      the seven AS global native-function singletons (real cbCallMethod_* bodies)
// Returns 0 (the console's r3 == the final ReleaseValues result, dead in every caller).
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
extern AptValue* gpAptNoneValue;              // off_8324D814 (AptGlobals.cpp SECOND view of the `undefined` slot)
extern AptValue* gpAptRootTargetMC;           // dword_8324D830 (the root-prototype mirror store)
extern AptValue* gpAptDestroyedClipValue;     // dword_8324D818 (the MovieClip-prototype pin)
extern AptValue* gpAptCurrentTargetMC;        // dword_8324D818 (SECOND view of the same console slot)
extern AptNativeHash* gpAptClassRegistry;     // dword_8324E2D4 (AptObject.cpp)
extern AptValue* gpAptFnSetInterval;          // off_8324D828 (AptGlobals.cpp; AptCIH::objectMemberLookup reads it)
extern AptValue* gpAptFnClearInterval;        // off_8324D81C ("")
extern AptValue* gpAptFnIsNaN;                // off_8324D824 ("")
extern AptValue* gpAptFnUnescape;             // off_8324E1FC ("")
extern AptValue* gpAptFnEscape;               // off_8324D80C ("")
extern AptValue* gpAptFnBoolean;              // off_8324D74C ("")
extern const EAStringC gAptObjectClassName;   // &dword_8324E650 "Object"
extern const EAStringC gAptStringClassName;   // &dword_8324E6B4 "String"
extern const EAStringC gAptSpriteClassKey;    // dword_8324E640 "MovieClip"

// The six remaining builtin class-name constants (X360 unk_8324E5FC/E610/E618/E6BC/
// E6D4/E61C -- .data EAStringC constants X360-xref'd ONLY by the AS-globals bootstrap
// @0x82AF6B68, so homed file-local here). The name SET {Array, Date, TextFormat,
// Color, XML, Error} is grounded by the XB1 new-object dispatcher (sub_1408485A0's
// stricmp ladder: Array/String/Date/TextFormat/Color/MovieClip/XML/Error, + Object)
// and the B4Extern sibling's packed class-name rodata block; the per-address name
// ORDER within the six is unrecovered (no other xref exists on either platform) and
// is immaterial to the unordered hash install below (every entry gets the identical
// stub + prototype treatment).
static const EAStringC gAptArrayClassName     ("Array");        // unk_8324E5FC (Set slot 2)
static const EAStringC gAptDateClassName      ("Date");         // unk_8324E610 (Set slot 3)
static const EAStringC gAptTextFormatClassName("TextFormat");   // unk_8324E618 (Set slot 4)
static const EAStringC gAptColorClassName     ("Color");        // unk_8324E6BC (Set slot 5)
static const EAStringC gAptXMLClassName       ("XML");          // unk_8324E6D4 (Set slot 7)
static const EAStringC gAptErrorClassName     ("Error");        // unk_8324E61C (Set slot 8)

// off_8324E378 -- the ASSetPropFlags native-fn singleton slot (X360-xref'd only by
// this TU's init/teardown pair, so file-local).
static AptValue* gpAptFnASSetPropFlags = nullptr;

// The registerClass native is AptObject::RegisterClassNative (AptObject.cpp;
// X360 sub_82AF6A38); its declaration arrives with AptObject.h below.

int AptValueInitialize()
{
    if (gpUndefinedValue == nullptr)
        gpUndefinedValue = new AptNone();   // pooled operator new (gpNonGCPoolManager)
    gpAptNoneValue = gpUndefinedValue;      // off_8324D814: AptGlobals.cpp's second view of
                                            // the SAME console slot (one object, two names)

    AptBoolean::Initialize();
    AptLookup::Initialize();
    AptRegister::Initialize();

    // off_8324E498 / off_8324E49C = 0 -- reset the cached default text/movie character
    // templates (rebuilt lazily by AptCharacterHelper; Shutdown frees them on teardown).
    AptCharacterHelper::spDefaultTextCharacter  = nullptr;
    AptCharacterHelper::spDefaultMovieCharacter = nullptr;

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

    // off_8324D82C -- the shared empty AptString (Create(&unk_820046A7) == Create("")),
    // GC-rooted then AddRef'd (asm @0x82B02A34..54: store; setGCRoot(slot, 1); vtbl[0]
    // on the reloaded slot).
    if (gpAptStringObject == nullptr)
    {
        gpAptStringObject = AptString::Create("");
        if (gpAptStringObject)
        {
            gpAptStringObject->setGCRoot(1);
            gpAptStringObject->AddRef();
        }
    }

    // dword_8324E2D4 = 0 -- clear the AS export-name class registry before the
    // AS-globals bootstrap runs (Object.registerClass lazily rebuilds it).
    gpAptClassRegistry = nullptr;

    // ---- sub_82AF6B68 -- the AS-globals bootstrap (tail-called; wired IN FULL) ----
    // Drain the shared deferred-release vector, install the NINE builtin class
    // natives into _global's hash (off_8324E380+8) -- each a fresh AptNativeFunction
    // wrapping the SHARED generic-constructor stub (the Hex-Rays resolve of every one
    // of the nine is cbCallMethod_ASSetPropFlags @0x82AD8448, the return-ok stub; the
    // classes' real behaviour lives in the prototypes + the engine value types) --
    // then seed each with a fresh AptPrototype: the FIRST entry ("Object",
    // &dword_8324E650) becomes the prototype ROOT (dword_8324E4EC ==
    // gpAptFunctionPrototypeRoot, mirrored by the console into dword_8324D830 ==
    // gpAptRootTargetMC), every other entry's __proto__ links to that root, and the
    // SIXTH entry's ("MovieClip", dword_8324E640) fresh prototype is ALSO pinned into
    // dword_8324D818 (both reconstruction views written). Class + prototype are
    // GC-rooted per entry, then the Object.registerClass native is wrapped LAST and
    // the vector is drained again on the way out.
    if (gpAptGlobalFallback != nullptr)
    {
        AptNativeHash* const pGlobals = gpAptGlobalFallback->GetNativeHashVirtual();
        if (pGlobals != nullptr && pGlobals->Lookup(gAptObjectClassName) == nullptr)
        {
            gpValuesToRelease->ReleaseValues();   // bootstrap-entry drain (off_8324E51C)

            // The nine-entry builtin class table, in the console's Set order
            // (E650, E5FC, E610, E618, E6BC, E640, E6D4, E61C, E6B4).
            const EAStringC* const lakNames[9] =
            {
                &gAptObjectClassName,       // dword_8324E650 "Object"    (slot 1: the root)
                &gAptArrayClassName,        // unk_8324E5FC   (slots 2-5/7-8: see the six-name note above)
                &gAptDateClassName,         // unk_8324E610
                &gAptTextFormatClassName,   // unk_8324E618
                &gAptColorClassName,        // unk_8324E6BC
                &gAptSpriteClassKey,        // dword_8324E640 "MovieClip" (slot 6: the D818 pin)
                &gAptXMLClassName,          // unk_8324E6D4
                &gAptErrorClassName,        // unk_8324E61C
                &gAptStringClassName,       // dword_8324E6B4 "String"
            };
            for (int li = 0; li < 9; ++li)
            {
                AptNativeFunction* const pCtor = AptExtObject::CreateNewAptFunction(
                    reinterpret_cast<AptExtFunctionPtr>(
                        &AptActionInterpreter::cbCallMethod_ASSetPropFlags));
                if (pCtor)
                    pGlobals->Set(*lakNames[li], pCtor);
            }

            for (int li = 0; li < 9; ++li)
            {
                AptValue* const pClass = pGlobals->Lookup(*lakNames[li]);
                if (pClass == nullptr)
                    continue;
                AptPrototype* const pProto = new AptPrototype();
                if (li == 5)
                {
                    // dword_8324D818 = the fresh MovieClip prototype (the console's
                    // pin; both reconstruction views of that one slot stay coherent).
                    gpAptDestroyedClipValue = pProto;
                    gpAptCurrentTargetMC    = pProto;
                }
                AptNativeHash* const pClassHash = pClass->GetNativeHashVirtual();
                if (pClassHash != nullptr)
                {
                    pClassHash->SetPrototype(pProto);
                    if (li == 0)
                    {
                        gpAptFunctionPrototypeRoot = pProto;   // dword_8324E4EC (the root)
                        gpAptRootTargetMC          = pProto;   // dword_8324D830 (the console's mirror)
                    }
                    else
                        pClassHash->Set__Proto__(gpAptFunctionPrototypeRoot);
                }
                pClass->setGCRoot(1);
                if (pProto)
                    pProto->setGCRoot(1);
                // (The MovieClip clip METHODS -- gotoAndPlay / play / getTextFormat /
                // ... -- are NOT prototype members: the console resolves them through
                // AptCIH::objectMemberLookup's SpriteMembersIndex recognizer, homed in
                // AptCIHMembers.cpp 2026-07-09.)
            }

            // off_8324D748 -- the Object.registerClass native (sub_82AF6A38, homed as
            // AptObject::RegisterClassNative in AptObject.cpp), wrapped LAST by the
            // console: new AptNativeFunction; store; setGCRoot(1); AddRef (vtbl[0] on
            // the reloaded slot -- asm 0x82AF7228..0x82AF7270).
            if (gpObjRegistrationFunc == nullptr)
            {
                gpObjRegistrationFunc = AptExtObject::CreateNewAptFunction(
                    reinterpret_cast<AptExtFunctionPtr>(&AptObject::RegisterClassNative));
                if (gpObjRegistrationFunc)
                {
                    gpObjRegistrationFunc->setGCRoot(1);
                    gpObjRegistrationFunc->AddRef();
                }
            }

            // The console resolves Object.registerClass through AptObject::
            // objectMemberLookup @0x82AD6480 (the vendor hierarchy is AptNativeFunction
            // : public AptObject -- apt_types.gen.h:510 -- so every class value
            // inherits the recognizer). FLAG: our AptNativeFunction sits beside
            // AptObject, so the native is installed as an ORDINARY member of the
            // Object builtin's hash -- observable-identical for AS lookups; revisit
            // if the value class tree is re-based onto AptObject.
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

            gpValuesToRelease->ReleaseValues();   // bootstrap-exit drain (off_8324E51C)
        }
    }

    // The colour-transform / matrix default stores the console makes next
    // (flt_82F7338C..98 = 255.0, flt_82F733A0..AC = 0.0, flt_8324E2B0..E2C4 =
    // {1,0,0,1,0,0}) are the gAptNullCXForm / gAptIdentityMatrix singletons --
    // const-initialised to those exact values in AptGlobals.cpp, so the runtime
    // re-store is subsumed by the static initialisation.

    // The seven AS global native-function singletons, in the console's build order:
    // each a fresh AptNativeFunction over its REAL cbCallMethod_* body (homed in
    // AptIntervalTimer.cpp / AptActionInterpreterBuiltins.cpp), stored, GC-rooted,
    // then AddRef'd (asm per slot: store; setGCRoot(v, 1); vtbl[0] on the reload).
    // The first six slots live in AptGlobals.cpp (AptCIH::objectMemberLookup
    // @0x82B0DF70 resolves the global function names through them); the seventh
    // (ASSetPropFlags, off_8324E378) is file-local above.
    struct SNativeFnInit { AptValue** mppSlot; AptExtFunctionPtr mpFunc; };
    const SNativeFnInit lakNativeFns[7] =
    {
        { &gpAptFnSetInterval,    reinterpret_cast<AptExtFunctionPtr>(&AptActionInterpreter::cbCallMethod_setInterval)    },  // off_8324D828
        { &gpAptFnClearInterval,  reinterpret_cast<AptExtFunctionPtr>(&AptActionInterpreter::cbCallMethod_clearInterval)  },  // off_8324D81C
        { &gpAptFnIsNaN,          reinterpret_cast<AptExtFunctionPtr>(&AptActionInterpreter::cbCallMethod_isNaN)          },  // off_8324D824
        { &gpAptFnUnescape,       reinterpret_cast<AptExtFunctionPtr>(&AptActionInterpreter::cbCallMethod_unescape)       },  // off_8324E1FC
        { &gpAptFnEscape,         reinterpret_cast<AptExtFunctionPtr>(&AptActionInterpreter::cbCallMethod_escape)         },  // off_8324D80C
        { &gpAptFnBoolean,        reinterpret_cast<AptExtFunctionPtr>(&AptActionInterpreter::cbCallMethod_boolean)        },  // off_8324D74C
        { &gpAptFnASSetPropFlags, reinterpret_cast<AptExtFunctionPtr>(&AptActionInterpreter::cbCallMethod_ASSetPropFlags) },  // off_8324E378
    };
    for (int li = 0; li < 7; ++li)
    {
        if (*lakNativeFns[li].mppSlot != nullptr)
            continue;   // idempotent re-init (the singletons survive until AptValueShutdown)
        AptNativeFunction* const pFn = AptExtObject::CreateNewAptFunction(lakNativeFns[li].mpFunc);
        *lakNativeFns[li].mppSlot = pFn;
        if (pFn)
        {
            pFn->setGCRoot(1);
            pFn->AddRef();
        }
    }

    // dword_8324E7AC..E7B8 = 0 -- the console zeroes a 4-dword block with no other
    // xref anywhere in the ARTIST export set (already-zero .bss scratch); no
    // reconstruction storage models it, so the clear has no visible effect here.

    // The function-final drain (the console's r3 == this ReleaseValues result, dead
    // in every caller; non-null on the live init path -- AptCommonInitialize ran).
    gpValuesToRelease->ReleaseValues();
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
// FLAG PC-platform leaf: the two interrupt-masked lwarx/stwcx. TAS spin locks on
// unk_8324E71C bracketing the walk (the same one-shot latch AptValueInitialize uses)
// are elided single-threaded (threading primitive, not an engine method). No null
// guards are added: the console's per-singleton dispatch is unconditional (only
// dword_8324E2D4 and dword_8324E2AC are guarded), transcribed verbatim.
//
// The seven AS native-function singletons (off_8324D828/D81C/D824/E1FC/D80C/D74C/
// E378) are the teardown side of the SAME singletons AptValueInitialize builds
// above: the first six live in AptGlobals.cpp (AptCIH::objectMemberLookup resolves
// the global names through them), the seventh (ASSetPropFlags) is file-local above.
// ===========================================================================

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

    // FLAG PC-platform leaf: first interrupt-masked TAS on unk_8324E71C elided (single-threaded).

    // off_8324E37C -- the _global extension object.
    gpAptGlobalExtensionObject->DestroyGCPointers();   // vtbl +0x28
    gpAptGlobalExtensionObject->Release();             // vtbl +0x04 (drops the boot AddRef)
    gpAptGlobalExtensionObject = nullptr;

    // FLAG PC-platform leaf: second interrupt-masked TAS on unk_8324E71C elided (single-threaded).

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

    // The seven AS global native-function singletons (built by AptValueInitialize).
    gpAptFnSetInterval->DestroyGCPointers();
    gpAptFnSetInterval->Release();
    gpAptFnSetInterval = nullptr;

    gpAptFnClearInterval->DestroyGCPointers();
    gpAptFnClearInterval->Release();
    gpAptFnClearInterval = nullptr;

    gpAptFnIsNaN->DestroyGCPointers();
    gpAptFnIsNaN->Release();
    gpAptFnIsNaN = nullptr;

    gpAptFnUnescape->DestroyGCPointers();
    gpAptFnUnescape->Release();
    gpAptFnUnescape = nullptr;

    gpAptFnEscape->DestroyGCPointers();
    gpAptFnEscape->Release();
    gpAptFnEscape = nullptr;

    gpAptFnBoolean->DestroyGCPointers();
    gpAptFnBoolean->Release();
    gpAptFnBoolean = nullptr;

    gpAptFnASSetPropFlags->DestroyGCPointers();
    gpAptFnASSetPropFlags->Release();
    gpAptFnASSetPropFlags = nullptr;

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
// FLAG PC-platform leaf: AptSetRenderThreadID/AptSetSimulationThreadID take an int the
// vendor GetThreadId ignores (the console threads r3 through them -- dead plumbing); the
// two interrupt-masked TAS spin locks (reached via the sub-calls, and the final
// unk_8324E6E0 latch clear) are single-threaded no-ops (threading primitives, not engine
// methods). The return is StringPool::ClearTemporaryPool's r3, void here -> 0.
// ===========================================================================
int AptCommonShutdown()
{
    // Record the sim/render thread ids (the console threads r3 through both; the arg is
    // unused on the vendor GetThreadId -- see AptSetRenderThreadID's PC-platform leaf).
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
    gpValuesToRelease->ReleaseValues();          // AptValueVector::ReleaseValues(off_8324E51C)
    AptValueVector* lpVec = gpValuesToRelease;   // v5 = off_8324E51C
    if (lpVec != nullptr)
    {
        // Family-(B) vector (AptValueVector.h): the +0 word (mnTop member) holds the
        // capacity; the item array is 4*capacity, the header is the 12-byte block.
        gpAptPseudoDataPool->Deallocate(lpVec->mppItems, sizeof(AptValue*) * lpVec->mnTop);  // 4 * *v5
        gpAptPseudoDataPool->Deallocate(lpVec, sizeof(AptValueVector));                       // 12
    }
    gpValuesToRelease = nullptr;   // off_8324E51C = 0

    // Clear the integer/float free pools (befriended) + the temporary string pool. The
    // console threads r3 through the two ClearPool calls (both void) into ClearTemporaryPool.
    AptInteger::ClearPool();
    AptFloat::ClearPool();
    AptStringPool_ClearTemporaryPool();   // StringPool::ClearTemporaryPool (its r3 -> result)

    gpAptCommonFuncsSlot = nullptr;   // dword_8324E4F0 = 0
    // FLAG PC-platform leaf: the final interrupt-masked TAS clearing the once-only init
    // guard (dword_8324E6E0) is a single-threaded no-op; the guard store is the visible reset.
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

    intptr_t liTop = AptMath::ClipStackInit(128);    // x64: pointer-width (the AptMath.h clip-stack widening)
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
// x64 (the pervasive width rule): the pool holds pointer-width slots
// (AptRenderInitialize allocated sizeof(void*) * count), so the drain strides by
// void* and the Deallocate size mirrors that Allocate size -- the console's literal
// `4 * count` byte math is the 32-bit-pointer form of the same walk.
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

    // dword_82F733EC (config +0x34, word 13) -- the AptLookup fixed-pool size. Same
    // treatment: the console reads the config word in place (AptLookup::Initialize
    // @0x82AE7E88 reads +0x34); published into gAptLookupPoolSize (AptGlobals.cpp)
    // so the pool build sees the configured count (default 128).
    gAptLookupPoolSize = static_cast<int>(lpParams[13]);

    // if (!off_82F733B0) off_82F733B0 = &unk_82F72FF8.
    if (gpAptConfigDefault == nullptr)
        gpAptConfigDefault = &gAptEmptyDefaultSentinel[0];

    // byte_8324E392 = 0; byte_8324E393 = params byte[0x3D].
    gAptDefaultTextMouseWheelEnabled = false;
    gbAptUpdateFlag393 = reinterpret_cast<const unsigned char*>(lpParams)[0x3D];

    // if (!dword_8324E820) dword_8324E820 = sub_82AD9030 (the DOGMA sized-free hook).
    if (gpAptGCTableFree == nullptr)
        gpAptGCTableFree = &AptDogmaFreeSizedHook;

    // dword_8324E4E8 -- the GC reference-registration callback: the console ships the
    // slot .data-initialised to AptGC::sReferenceRegistrationCb @0x82AD9C80 (its
    // RegisterReferences consumers call through it UNGUARDED -- e.g. AptNativeHash::
    // RegisterReferences @0x82ADA3C0 -- and the XB1 partial sweep sub_140832E70
    // save/stamps/restores the same slot, qword_141479F80 = sub_14085CB40, around its
    // mark walk). The boot install reproduces that never-null initialised state.
    if (AptValue::sReferenceRegistrationCb == nullptr)
        AptValue::sReferenceRegistrationCb = &AptGC::sReferenceRegistrationCb;

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

    // AptValueInitialize() -- the full AS value-singleton bootstrap (its body above).
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

    // dword_8324D810 -- construct the saved-input checkpoint list EMPTY. The console
    // pool-allocates 28 bytes: the AptFileSavedInputStateVector {size, cap, begin}
    // header with begin aimed at its inline 2-element SBO buffer, each inline element
    // seeded {&unk_82F72FF8 (the empty EAStringC sentinel), state 0}. The x64
    // reconstruction de-SBOs the container (AptSavedInputCheckpoints.h), so the
    // default ctor's {0, 0, null} is the same empty state -- the first insert
    // re-allocates either way. Published into gpAptSavedInputCheckpoints, the list
    // AptLinker::Update + the saved-input replay tick drive.
    void* lpListMem = gpAptPseudoDataPool->Allocate(sizeof(AptFileSavedInputStateVector));   // console 28; x64 sizeof
    gpAptSavedInputCheckpoints = lpListMem
                                     ? new (lpListMem) AptFileSavedInputStateVector()
                                     : nullptr;

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
// NB (no lock is elided here -- the console itself brackets nothing on this path).
// `a1` is AptValueShutdown's unused arg (threaded r3). The return is AptCommonShutdown's
// result, or the interpreter-shutdown r3 leftover (void) when the common teardown is
// skipped -> 0.
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

    // Free the saved-input checkpoint list (dword_8324D810). StringAsVec @0x82AF83F8 is
    // the AptFileSavedInputStateVector destructor -- Clear() releases the elements + the
    // heap backing -- then the header block goes back to the pool (console 28 bytes;
    // x64 sizeof).
    AptFileSavedInputStateVector* lpList = gpAptSavedInputCheckpoints;   // v10 = dword_8324D810
    if (lpList != nullptr)
    {
        lpList->Clear();                                                                 // StringAsVec(v10)
        gpAptPseudoDataPool->Deallocate(lpList, sizeof(AptFileSavedInputStateVector));   // Deallocate(pool, v10, 28)
    }
    gpAptSavedInputCheckpoints = nullptr;   // dword_8324D810 = 0

    // The GC's full value-pool clean (the console r3 into it is the dead reap/Deallocate
    // leftover; CleanAll takes no args).
    AptGC::CleanAll();

    // Drain the shared common-init deferred-release vector (off_8324E51C), unconditional
    // (non-null on the live path).
    gpValuesToRelease->ReleaseValues();   // AptValueVector::ReleaseValues(off_8324E51C)

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
