// AptRenderLinkStubs.cpp -- PC link stubs for the Apt engine: host-callback slots that are
// un-installed on PC, the single-threaded EA::Thread / EA::Jobs surface, the rw::core::filesys
// entry points the DeviceManager replay path bypasses, and a few Apt helpers with no other home.
#include "types.hpp"
#include "SDKs/EATech/Apt/AptActionDefineFunction2.h"
#include "SDKs/EATech/Apt/AptActionTryCatchFinallyBlock.h"
#include "SDKs/EATech/Apt/AptKeyMembersIndex.h"
#include "SDKs/EATech/Apt/AptMath.h"
#include "SDKs/EATech/Apt/AptObjectIndex.h"
#include "SDKs/EATech/Apt/AptTextFormatMembersIndex.h"
#include "SDKs/EATech/Apt/AptTextMembersIndex.h"
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"
#include "SDKs/EATech/Apt/DogmaAllocator.h"
#include "SDKs/EATech/include/Apt/Apt.h"
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptActionQueue.h"
#include "SDKs/EATech/include/Apt/AptActionQueueC.h"
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"
#include "SDKs/EATech/include/Apt/AptArray.h"
#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCIHNativeFunctionHelper.h"
#include "SDKs/EATech/include/Apt/AptCIHNone.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterDynamicText.h"
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterLevelInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterMorph.h"
#include "SDKs/EATech/include/Apt/AptCharacterMorphInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterShape.h"
#include "SDKs/EATech/include/Apt/AptCharacterShapeInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"
#include "SDKs/EATech/include/Apt/AptCharacterStaticText.h"
#include "SDKs/EATech/include/Apt/AptCharacterStaticTextInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterTextInst.h"
#include "SDKs/EATech/include/Apt/AptConstFile.h"
#include "SDKs/EATech/include/Apt/AptDate.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"
#include "SDKs/EATech/include/Apt/AptDisplayList.h"
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"
#include "SDKs/EATech/include/Apt/AptError.h"
#include "SDKs/EATech/include/Apt/AptExtObject.h"
#include "SDKs/EATech/include/Apt/AptFile.h"
#include "SDKs/EATech/include/Apt/AptFileSavedInputState.h"
#include "SDKs/EATech/include/Apt/AptFrameStack.h"
#include "SDKs/EATech/include/Apt/AptGC.h"
#include "SDKs/EATech/include/Apt/AptGlobal.h"
#include "SDKs/EATech/include/Apt/AptGlobalExtensionObject.h"
#include "SDKs/EATech/include/Apt/AptIntervalTimer.h"
#include "SDKs/EATech/include/Apt/AptKey.h"
#include "SDKs/EATech/include/Apt/AptLinker.h"
#include "SDKs/EATech/include/Apt/AptLinkerThingy.h"
#include "SDKs/EATech/include/Apt/AptListenerSlotList.h"
#include "SDKs/EATech/include/Apt/AptLoader.h"
#include "SDKs/EATech/include/Apt/AptMathObj.h"
#include "SDKs/EATech/include/Apt/AptMovie.h"
#include "SDKs/EATech/include/Apt/AptMovieClip.h"
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptObject.h"
#include "SDKs/EATech/include/Apt/AptPrototype.h"
#include "SDKs/EATech/include/Apt/AptPseudoCIH.h"
#include "SDKs/EATech/include/Apt/AptPseudoData.h"
#include "SDKs/EATech/include/Apt/AptPseudoDisplayList.h"
#include "SDKs/EATech/include/Apt/AptRenderHooks.h"
#include "SDKs/EATech/include/Apt/AptRenderItem.h"
#include "SDKs/EATech/include/Apt/AptRenderItemAnimation.h"
#include "SDKs/EATech/include/Apt/AptRenderItemButton.h"
#include "SDKs/EATech/include/Apt/AptRenderItemCustomControl.h"
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"
#include "SDKs/EATech/include/Apt/AptRenderItemLevel.h"
#include "SDKs/EATech/include/Apt/AptRenderItemMorph.h"
#include "SDKs/EATech/include/Apt/AptRenderItemShape.h"
#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"
#include "SDKs/EATech/include/Apt/AptRenderItemStaticText.h"
#include "SDKs/EATech/include/Apt/AptRenderManagerItem.h"
#include "SDKs/EATech/include/Apt/AptRenderManagerQueue.h"
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"
#include "SDKs/EATech/include/Apt/AptSavedInputCheckpoints.h"
#include "SDKs/EATech/include/Apt/AptScriptColour.h"
#include "SDKs/EATech/include/Apt/AptScriptFunction1.h"
#include "SDKs/EATech/include/Apt/AptScriptFunction2.h"
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"
#include "SDKs/EATech/include/Apt/AptScriptFunctionByteCodeBlock.h"
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"
#include "SDKs/EATech/include/Apt/AptSingleListPolicy.h"
#include "SDKs/EATech/include/Apt/AptSound.h"
#include "SDKs/EATech/include/Apt/AptStage.h"
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"
#include "SDKs/EATech/include/Apt/AptStd/AptRect.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptTarget.h"
#include "SDKs/EATech/include/Apt/AptTextFormat.h"
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include "SDKs/EATech/include/Apt/AptValue/AptExtern.h"
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"
#include "SDKs/EATech/include/Apt/AptValue/AptLookup.h"
#include "SDKs/EATech/include/Apt/AptValue/AptNone.h"
#include "SDKs/EATech/include/Apt/AptValue/AptRegister.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"
#include "SDKs/EATech/include/Apt/AptValueFactory.h"
#include "SDKs/EATech/include/Apt/AptValueWithHash.h"
#include "SDKs/EATech/include/Apt/AptXml.h"
#include "SDKs/EATech/include/Apt/AptXmlNode.h"

// ===================================================================================
// Engine link-stub headers. These are the EXISTING decls the Apt code already references; included so each stub matches the
// canonical signature/mangling exactly. NOTE: the reconstructed BrnEAThreadX360.h is
// deliberately NOT pulled (it declares EA::Thread::GetThreadId() returning ThreadId
// (void*), which would collide with the int-returning EA::Thread::GetThreadId() the Apt
// call sites reference -- AptCharacterAnimation.cpp); the EA::Thread surface is therefore
// declared minimally inline below. job_thread.h / local_backend.h are likewise NOT pulled
// (they transitively include BrnEAThreadX360.h); their two symbols are declared minimally.
// ===================================================================================
// NOTE: rwcore/filesys/device.h is deliberately NOT included -- it pulls
// BrnEAThreadX360.h (declaring EA::Thread::GetThreadId() returning ThreadId/void*), which
// collides with the int-returning EA::Thread::GetThreadId() the Apt call sites reference.
// The handful of rw::core::filesys types stubbed below are declared minimally instead.
#include "coreallocator/icoreallocator_interface.h"       // EA::Allocator::ICoreAllocator
#include "SDKs/EATech/eajobs/job_types.h"                 // EA::Jobs enums + Detail::SchedulerBackend + Param
#include "SDKs/EATech/eajobs/entry_point.h"               // EA::Jobs::EntryPoint
#include "SDKs/EATech/eajobs/event.h"                     // EA::Jobs::Event
#include "SDKs/EATech/eajobs/job.h"                       // EA::Jobs::Job
#include "SDKs/EATech/eajobs/job_thread_handle.h"         // EA::Jobs::JobThreadHandle / JobThreadParameters
#include "SDKs/EATech/include/Nicotine/SnapshotMixer.hpp" // Nicotine::SnapshotMixer
#include "SDKs/EATech/include/Nicotine/SnapshotChannel.hpp" // Nicotine::SnapshotVolumeCurve
#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"    // NFSMixMaster
#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"       // NFSMixMap (AssignSFXCallbacks forward)
#include "SDKs/EATech/include/NFSMix/NFSMixMapState.hpp"  // NFSMixMapState (the CreateMainMapState builder chain)
#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp"   // stMixMapHeader / stMixMapStateHdr (serialized MixMap blob records)
#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp"  // g_pMixerAllocator (off_83250004)
#include <cstring>                                         // std::memset (SnapshotMixer::InitSnapshots)


    // host extern-object member-GET callback slot (X360 dword_8324E858); the host installs it for
    // AptVFT_Extern (type-11) script objects -- none are registered on the PC title path, so the
    // un-installed slot faithfully answers null (matches the X360 null fn-ptr slot, not an engine stub).
    AptValue* AptExtern_GetMember(const char* szName) { return nullptr; }   // FLAG PC-platform leaf
    int GetThreadId() { return 0; }   // FLAG PC-platform leaf: single-threaded PC (one thread id)
    uint32_t AptCurrentThreadId() { return 0; }   // FLAG PC-platform leaf: single-threaded PC (one thread id)
    // host extern-object member-SET callback slot (X360 dword_8324E854); un-installed on the PC
    // title path (no type-11 extern objects) -> faithful no-op, matching the X360 null fn-ptr slot.
    void      AptExtern_SetMember(const char* szName, const char* szValue) {}   // FLAG PC-platform leaf
    // FLAG deferred: the real AptCharacterAnimation::ExecuteInitActions (AptCharacterAnimation.cpp)
    // is homed, but its init-action VM path runs away at boot (a flood of mkitem instantiation)
    // until the import->AptFile->embedded-movie chase is homed. Keep this no-op until that
    // sub-record path lands, THEN swap the AptMovie::doFrameControls tag-8 caller to the member.
    void  AptExecuteInitActionsGate(void* pAnim, void* pCIH, int nId) {}
    void  AptFreeFontUnit(void* pUnit) {}   // FLAG PC-platform leaf: host render-unit free callback (un-installed on PC)
    void  AptFreeRenderingUnit(void* pUnit) {}   // FLAG PC-platform leaf: host render-unit free callback (un-installed on PC)
    // Host URL-fetch callback slot (dword_8324E84C/..850), null on the PC title path (no host
    // loadVariables installed) -- the same host boundary as AptExtern_SetMember above.
    // FLAG PC-platform leaf: host callback slot, faithfully null on PC.
    AptValue* AptApt_LoadVariablesFetch(const char* pUrl) { return 0; }   // dword_8324E84C/850 un-installed -> null

    // AptApt_GetDragTargetTranslate -- the drag target's clip matrix translation: CIH ->
    // character inst -> render item position matrix (GetPositionMatrixConst carries the
    // console's null-matrix -> identity fallback), tx/ty out.
    void AptApt_GetDragTargetTranslate(AptValue* pDragTarget, float* pOutX, float* pOutY)
    {
        const AptMatrix* pPos = static_cast<AptCIH*>(pDragTarget)->GetCharacterInst()
                                    ->GetRenderItem()->GetPositionMatrixConst();
        *pOutX = pPos->tx;   // matrix +0x10 (console v11[4])
        *pOutY = pPos->ty;   // matrix +0x14 (console v11[5])
    }

    // ---- AptCIH GeneralisedProcess gate + callback statics (bEarlyReturn / sCIHProcessCb[0..2] /
    // nTreeDepth): boot zero/null on the console exactly as here. AptUpdate.cpp installs/swaps
    // the three callbacks around its process pass and AptCIHBehaviour.cpp reads them.
    bool AptCIH_sbGeneralisedProcessEarlyReturn = false;   // bEarlyReturn (boot 0)
    unsigned int (*AptCIH_sCIHProcessCb)(AptCIH*, AptCIH*, void*)  = nullptr;   // dword_8324E41C
    unsigned int (*AptCIH_sCIHProcessCb1)(AptCIH*, AptCIH*, void*) = nullptr;   // dword_8324E420
    unsigned int (*AptCIH_sCIHProcessCb2)(AptCIH*, AptCIH*, void*) = nullptr;   // dword_8324E424
    int  AptCIH_snGeneralisedProcessTreeDepth = 0;   // nTreeDepth (boot 0)

    // The saved-input REPLAY driver: drains the recorded input stream instead of live-ticking.
    // Gated on gbAptSavedInputActive (boot 0), so the empty body is unreachable until a host
    // arms the replay.
    void AptUpdateReplaySavedInputs(int, int) {}

    // Host debug-output sink (console dword_8324E82C, a printf-style hook the host installs).
    // FLAG PC-platform leaf: host debug sink, faithfully a no-op until the host wires it.
    void (*gpAptHookTraceFn)(const char* szFormat, const char* szMessage) = nullptr;   // dword_8324E82C
    void AptHook_Trace(const char* szFormat, const char* szMessage)
    {
        if (gpAptHookTraceFn)
            gpAptHookTraceFn(szFormat, szMessage);
    }

    // AptKeyManagerAddListener -- the Key-listener registration tail of sMethod_addListener:
    // scan the director's mListenerSet (bound = mnCapacity); already present -> no-op; else the
    // shared set add: head = count+1, probe forward from slots[head] for the first free slot
    // (wrapping at capacity), store the listener and AddRef it. The modulo probe is the
    // committed sibling idiom (AptCIHMembers.cpp AddNodeToInputSet): identical slot choice for
    // head < cap, in-bounds where the console's raw slots[cap] read is UB.
    void AptKeyManagerAddListener(AptValue* pListener)
    {
        AptAnimationTargetSet* const pSet = &gpAptTarget->GetAnimationTarget()->mListenerSet;

        const u32 luCap = pSet->mnCapacity;                    // lhz +2
        for (u32 lu = 0; lu < luCap; ++lu)
            if (pSet->mppSlots[lu] == pListener)               // membership scan @0x82ADC770
                return;
        if (luCap == 0)
            return;                                            // un-built set: nothing to add into

        const u16 nHead = static_cast<u16>(pSet->mnCount + 1u);
        pSet->mnCount = nHead;                                 // sth head (stored before the probe)
        u32 luNext = static_cast<u32>(nHead) % luCap;
        u32 luScanned = 0u;
        while (luScanned < luCap && pSet->mppSlots[luNext] != nullptr)
        {
            luNext = (luNext + 1u) % luCap;                    // wrap at capacity
            ++luScanned;
        }
        if (pSet->mppSlots[luNext] == nullptr)
        {
            pSet->mppSlots[luNext] = pListener;
            pListener->AddRef();                               // vtbl[0] tail-call
        }
    }

    // AptKeyManagerRemoveListener -- the shared set remove over the same director mListenerSet:
    // empty (count 0) -> false; linear-scan the slots (bound = capacity) for pListener; on a hit
    // decrement the count, Release the slot's value and null the slot. True iff one was removed.
    bool AptKeyManagerRemoveListener(AptValue* pListener)
    {
        AptAnimationTargetSet* const pSet = &gpAptTarget->GetAnimationTarget()->mListenerSet;

        if (pSet->mnCount == 0)                                // lhz +0; beq -> 0
            return false;

        const u32 luCap = pSet->mnCapacity;                    // lhz +2
        u32 luIndex = 0;
        while (luIndex < luCap && pSet->mppSlots[luIndex] != pListener)
            ++luIndex;
        if (luIndex >= luCap)
            return false;                                      // not found

        pSet->mnCount = static_cast<u16>(pSet->mnCount - 1);   // sth (count-1)
        pSet->mppSlots[luIndex]->Release();                    // vtbl[1]
        pSet->mppSlots[luIndex] = nullptr;
        return true;
    }
    // FLAG PC-platform leaf: host async-stream cancel hook (dword_8324E83C) -- the
    // PC stream hook (AptLoaderStartAsyncLoad) loads synchronously, so nothing is
    // ever in flight to cancel; the empty body matches the un-installed slot.
    void AptLoaderCancelAsyncLoad(void* pDataBlock) {}
    void Mutex_Lock(void* pMutex, void* pName) {}   // FLAG PC-platform leaf: single-threaded PC (no lock needed)
    void Mutex_Unlock(void* pMutex) {}   // FLAG PC-platform leaf: single-threaded PC (no lock needed)

// ===================================================================================
// ENGINE link-stubs -- off the PC render-critical path. The PC bring-up is
// single-threaded; the bundle FS uses the existing DeviceManager replay path (NOT
// rw::core::filesys). Each is a deliberate bring-up bridge; homed faithfully once hit.
// ===================================================================================

// ---- rw::core::filesys -------------------------------------------------------------
// FLAG: PC-simplification. The faithful async bundle path IS rw::core::filesys per
// async-filesystem-blueprint; PC uses the DeviceManager replay FS instead, so these
// scheduler/handle/manager entry points are stubbed until that faithful path is wired.
// Types declared minimally (NOT via device.h) so BrnEAThreadX360.h is not transitively
// pulled; the minimal decls reproduce only what each stubbed symbol's mangling needs.
namespace rw { namespace core { namespace filesys {

    class  Device;
    struct AsyncOp;

    // Minimal Handle matching the asyncop.h layout the ctor zero/stores.
    struct Handle
    {
        Handle(const char* lpcPath, u32 luPositionHi, Device* lpDevice);
        u32   mField0;
        u32   mField1;
        u32   mbIsOpen;
        u32   mField3;
        void* mpDevice;
    };

    // Minimal Manager / Device / DeviceDriverVTable -- just the stubbed members.
    struct Manager
    {
        Device* RegisterDevice(const void* lpDeviceDesc, int liFlags);
        int     UnregisterDevice();
    };

    class Device
    {
    public:
        static Device* GetInstance(const char* lpcPath, char* lpScratch);
        int Wait(AsyncOp* lpOp, const void* lpTimeout);
        int InsertOp(AsyncOp* lpOp);
        int ChangeOpPriority(AsyncOp* lpOp, int liPriority);
    };

    struct DeviceDriverVTable
    {
        void* mpfnSlot0;
        void* mpfnOpen;
        void* mpfnClose;
        void* mapfnReserved0C[7];
        void* mpfnGetBlockSize;
    };

    // ctor: zero/store args into the X360 field order.
    Handle::Handle(const char* lpcPath, u32 luPositionHi, Device* lpDevice)
        : mField0(0)
        , mField1(luPositionHi)
        , mbIsOpen(0)
        , mField3(0)
        , mpDevice(lpDevice)
    {
        (void)lpcPath;   // FLAG link-stub: path not opened (DeviceManager replay path used)
    }

    Device* Manager::RegisterDevice(const void* lpDeviceDesc, int liFlags)   // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path
    { (void)lpDeviceDesc; (void)liFlags; return nullptr; }

    int Manager::UnregisterDevice() { return 0; }   // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path

    Device* Device::GetInstance(const char* lpcPath, char* lpScratch)        // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path
    { (void)lpcPath; (void)lpScratch; return nullptr; }

    int Device::Wait(AsyncOp* lpOp, const void* lpTimeout)                    // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path
    { (void)lpOp; (void)lpTimeout; return 0; }

    int Device::InsertOp(AsyncOp* lpOp) { (void)lpOp; return 0; }             // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path

    int Device::ChangeOpPriority(AsyncOp* lpOp, int liPriority)               // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path
    { (void)lpOp; (void)liPriority; return 0; }

    // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path (off_8327F078 manager unused)
    Manager* gpFileSysManager = nullptr;
    // extern: namespace-scope const defaults to internal linkage; force external.
    // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path (zero-init driver vtable)
    extern const DeviceDriverVTable gDeviceDriverVTable = {};

}}}

// ---- rw::collision -----------------------------------------------------------------
// FLAG: collision, not on the Apt render path. No shared header declares
// rw::collision::VolumeLineQuery (it lives inside volumelinequery.cpp); a minimal
// matching declaration is provided so the GetIntersections() mangling is exact.
namespace rw { namespace collision {

    class VolumeLineQuery { public: int GetIntersections(); };   // FLAG link-stub (minimal decl for mangling)
    int VolumeLineQuery::GetIntersections() { return 0; }        // FLAG link-stub

}}

// ---- EA::Thread --------------------------------------------------------------------
// FLAG: single-threaded PC bring-up. Locks/thread-ids/TLS/runnable are no-ops. Declared
// minimally here (NOT via BrnEAThreadX360.h) so the int-returning GetThreadId() the Apt
// call sites reference is the symbol defined, and so the EATech reconstructed/vendor
// EAThread headers are not transitively pulled.
namespace EA { namespace Thread {

    // Minimal matching decls for the class members the Apt path references.
    struct IRunnable
    {
        virtual ~IRunnable();
        virtual intptr_t Run(void* pContext) = 0;   // pure: shape only; never instantiated here
    };

    class Thread
    {
    public:
        enum Status { kStatusNone = 0, kStatusRunning = 1, kStatusEnded = 2 };
        Status WaitForEnd(intptr_t* pThreadReturnValue, const int* pTimeoutAbsolute);
    };

    // Minimal ThreadLocalStorage with an INLINE trivial ctor so gAptTargetTls needs no
    // out-of-line ctor symbol (FLAG: zeroed storage; faithful ctor does TlsAlloc).
    class ThreadLocalStorage
    {
    public:
        ThreadLocalStorage() : mTlsIndex(0) {}   // inline -- no external ctor symbol
        bool  SetValue(const void* pData);
        void* GetValue();
        u32   mTlsIndex;
    };

    void Mutex_Lock(void* pMutex, void* pName)  { (void)pMutex; (void)pName; }   // FLAG PC-platform leaf: single-threaded PC (no lock needed)
    void Mutex_Unlock(void* pMutex)             { (void)pMutex; }                // FLAG PC-platform leaf: single-threaded PC (no lock needed)
    int  GetThreadId()                          { return 0; }                    // FLAG PC-platform leaf: single-threaded PC (one thread id)

    Thread::Status Thread::WaitForEnd(intptr_t* pThreadReturnValue, const int* pTimeoutAbsolute)
    { (void)pThreadReturnValue; (void)pTimeoutAbsolute; return kStatusEnded; }   // FLAG PC-platform leaf: single-threaded PC (thread already ended)

    // One file-scope slot mirroring the single TLS value the bring-up needs.
    static void* gThreadLocalStorageSlot = nullptr;   // FLAG PC-platform leaf: single-threaded TLS (one slot)
    bool  ThreadLocalStorage::SetValue(const void* pData)
    { gThreadLocalStorageSlot = const_cast<void*>(pData); return true; }         // FLAG PC-platform leaf: single-threaded TLS (one slot)
    void* ThreadLocalStorage::GetValue() { return gThreadLocalStorageSlot; }     // FLAG PC-platform leaf: single-threaded TLS (one slot)

    IRunnable::~IRunnable() {}   // FLAG PC-platform leaf: single-threaded PC (empty virtual dtor)

}}

// The Apt animation-unresolve current-target TLS object (unk_8324E814). The linker
// wants it at GLOBAL scope (?gAptTargetTls@@3V...), NOT in EA::Thread -- it is a global
// variable whose TYPE is EA::Thread::ThreadLocalStorage. Default-constructed via the
// inline ctor -- FLAG: zeroed storage, no TlsAlloc (single-threaded bring-up).
// FLAG PC-platform leaf: single-threaded TLS (zeroed storage; the faithful ctor does TlsAlloc)
EA::Thread::ThreadLocalStorage gAptTargetTls;

// ---- EA::Jobs ----------------------------------------------------------------------
// FLAG: jobs run synchronously on the main thread (PC bring-up). The two LocalBackend
// worker entry points (JobInstance::Run / JobThread::Start) are declared minimally to
// avoid pulling job_thread.h / local_backend.h (which transitively include
// BrnEAThreadX360.h and would collide with the int GetThreadId() above).
namespace EA { namespace Jobs {

    Event::Event() {}   // FLAG PC-platform leaf: synchronous jobs on PC (no event state)

    JobThreadHandle::JobThreadHandle(Detail::SchedulerBackend* pBackend, u32 uHandle)   // FLAG PC-platform leaf: synchronous jobs on PC
    { (void)pBackend; (void)uHandle; }
    JobThreadHandle::JobThreadHandle() {}   // FLAG PC-platform leaf: synchronous jobs on PC

    // The vendor accessors (declaration-only in entry_point.h): trivial member reads,
    // inlined on the console.
    JobAffinity    EntryPoint::GetAffinity()    const { return mAffinity; }
    JobEnvironment EntryPoint::GetEnvironment() const { return mEnvironment; }
    JobPriority    EntryPoint::GetPriority()    const { return mPriority; }

    // The mDependencies twin of Job::GetNumDependents (job.cpp): this node's bucket count
    // + the overflow chain's ListSize.
    int Job::GetNumDependencies() const
    {
        u32 luOverflow = 0;
        if (mDependencies.mNext)
            luOverflow = mDependencies.mNext->ListSize();
        return static_cast<int>(mDependencies.mSize + luOverflow);
    }

    // Minimal LocalBackend-scope decls for the two worker entry points (jobs run
    // synchronously on the main thread, so both are no-ops).
    namespace LocalBackend {
        class LocalBackend;
        struct JobInstance { void Run(); };
        class  JobThread   { public: void Start(const EA::Jobs::JobThreadParameters* pParameters, LocalBackend* pBackend); };

        void JobInstance::Run() {}   // FLAG PC-platform leaf: synchronous jobs on PC (main-thread run)
        void JobThread::Start(const EA::Jobs::JobThreadParameters* pParameters, LocalBackend* pBackend)   // FLAG PC-platform leaf: synchronous jobs on PC (no worker threads)
        { (void)pParameters; (void)pBackend; }
    }

}}

// ---- EA::Allocator -----------------------------------------------------------------
namespace EA { namespace Allocator {
    // FLAG link-stub: null default allocator. Prefer nullptr per the bring-up plan; a
    // non-null is only needed if an immediate deref happens (then home a file-scope one).
    ICoreAllocator* ICoreAllocator::GetDefaultAllocator() { return nullptr; }   // FLAG link-stub
}}
