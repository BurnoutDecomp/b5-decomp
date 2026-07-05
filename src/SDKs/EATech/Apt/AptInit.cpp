// ===========================================================================
// EATech Apt -- AptInit.cpp: the Apt runtime BRING-UP entry points.
//
// Decompiled store-for-store from the X360 ARTIST.XEX (the `assembly` field is
// authoritative for the call order + which register/global each store touches).
// These are the functions CgsGui::AptAux::InitializeApt @0x82848E50 chains to
// stand up the Apt runtime; the host bring-up (BrnAptRuntimeBringUp.cpp) previously
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
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"         // AptNativeFunction (complete type for the AptValue* store)
#include "SDKs/EATech/include/Apt/AptNativeHash.h"             // the _global hash (builtin class install)
#include "SDKs/EATech/include/Apt/AptPrototype.h"               // AptPrototype (builtin prototype seeding)
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"        // AptBoolean::Initialize (befriended)
#include "SDKs/EATech/include/Apt/AptValue/AptLookup.h"         // AptLookup::Initialize
#include "SDKs/EATech/include/Apt/AptValue/AptRegister.h"       // AptRegister::Initialize + gnAptRegisterCount
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

// ---- the AptTarget context (AptUpdateInitialize's a2 branch) ---------------
#include "SDKs/EATech/include/Apt/AptTarget.h"                  // AptTarget (ctor) + AptChangeTargetInstance

// ---- EA::Thread (record the sim/render thread ids + the shared mutex) -------
#include "eathread/eathread.h"                                  // EA::Thread::GetThreadId / ThreadId
#include "eathread/eathread_mutex.h"                            // EA::Thread::Mutex (vendor)

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
    // FLAG: interrupt-masked lwarx/stwcx. TAS elided single-threaded.
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

// The registerClass native body (AptObject.cpp; X360 sub_82AF6A38).
AptValue* AptApt_RegisterClassNative(AptValue* pContext, int nNumParams);

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
    // AptApt_RegisterClassNative in AptObject.cpp) in an AptNativeFunction and pins
    // it. Installed here (the same boot moment); FLAG: the REST of sub_82AF6B68 (the
    // per-builtin-class AptPrototype seeding over _global's hash entries) stays
    // deferred with the AS-builtin class registry.
    if (gpObjRegistrationFunc == nullptr)
    {
        gpObjRegistrationFunc = AptExtObject::CreateNewAptFunction(
            reinterpret_cast<AptExtFunctionPtr>(&AptApt_RegisterClassNative));
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
            }
        }
    }

    // FLAG (deferred): the remaining singletons -- the 7 AS native-function singletons
    // (setInterval/clearInterval/isNaN/unescape/escape/boolean/ASSetPropFlags) + the
    // SIX un-named builtin table entries of sub_82AF6B68 -- stay null until homed.
    return 0;
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
