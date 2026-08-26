// ============================================================================
// CgsSoundLogicModule.cpp -- CgsSound::Logic::Module runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   Module::Module()                @ 0x827E0FD0
//   Module::Construct()             @ 0x826C4230
//   Module::Prepare(...)            @ 0x826C42A8
//   Module::Update(...)             @ 0x826EAA50
//   Module::ProcessMessageQueue     @ 0x826C45A8
//   Module::AttachBuffers           @ 0x8268D3F0
//   Module::DetachBuffers           @ 0x8268D400
//   operator++(EModulePrepareStage&,int) @ 0x82681D30
//
// (2026-08-25, faithful-audio-engine phase B2: the wave-5 raw byte-image ctor is
// RETIRED -- the class now has the real ModuleSingleBuffered base + the DWARF
// member list, so the flattened placement-stores collapse into ordinary member
// construction. See the header banner for the console layout closure.)
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"    // IOBuffer Lock/Unlock
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"   // Io::MessageHeader (Notify dispatch)

namespace CgsSound
{
namespace Logic
{

namespace
{
    // X360 dword_82FFBC14 -- the process-wide Module instance counter Construct
    // post-increments into muInstanceIndex.
    u32 gu32ModuleInstanceCounter = 0;
}

// X360 off_82FFB954 -- the file-scope mirror of the module's adopted allocator
// (Prepare's prologue stores it beside mpAllocator; the sound state/effect
// deleting-destructor thunks free through it).
rw::IResourceAllocator* gpModuleAllocator = 0;

// ---------------------------------------------------------------------------
// Module::Module() @0x827E0FD0
//
// With the real base + members, the X360 ctor's flattened stores ARE the member
// construction sequence: the ModuleSingleBuffered base (vtable + the two
// RWMutexes @+0x10/+0x118), mPlaybackModule (@+0x238, its own ctor), and
// mEnvironment's embedded MicrophoneSystem (@+0x29A0) + DynamicMixer (@+0x2C30,
// with the +0x2C50 mpEnvironment zero). The console's mixer-vtable override
// (off_820CDC40) is the DynamicMixer subclass vtable, emitted by the compiler;
// the trailing +0x2C74 byte zero is mMessageQueue's count byte (established by
// Construct()'s queue Construct).
// ---------------------------------------------------------------------------
Module::Module()
    : CgsModule::ModuleSingleBuffered()
    , mpLogicInputBuffer(0)
    , mpLogicOutputBuffer(0)
    , muUniqueId(0)
    , muInstanceIndex(0)
    , mpAllocator(0)
    , mePrepareStage(eModulePrepareStart)
    , meReleaseStage(eModuleReleaseDone)
{
}

// ---------------------------------------------------------------------------
// Module::Construct() @0x826C4230
//   ModuleSingleBuffered::Construct(this)
//   +0x4C88 meReleaseStage = 5 (eModuleReleaseDone)   +0x2C70 mpAllocator = 0
//   +0x4C84 mePrepareStage = 0                        +0x230 muUniqueId = 0
//   +0x234 muInstanceIndex = dword_82FFBC14++
//   +0x228/+0x22C buffer pointers = 0                 +4 mbIsNewModule = 1
//   VariableEventQueue<8192,16>::Construct(+0x2C74)
// ---------------------------------------------------------------------------
void Module::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    meReleaseStage = eModuleReleaseDone;
    mpAllocator    = 0;
    mePrepareStage = eModulePrepareStart;

    muUniqueId      = 0;
    muInstanceIndex = gu32ModuleInstanceCounter++;

    mpLogicInputBuffer  = 0;
    mpLogicOutputBuffer = 0;

    mbIsNewModule = true;

    mMessageQueue.Construct();
}

// ---------------------------------------------------------------------------
// Module::Prepare @0x826C42A8  (DWARF :82)
//
// Pre-switch prologue (every call): adopt the allocator (assert :107; +0x2C70
// and the off_82FFB954 mirror), Prepare the message queue, assert the buffers
// (:114/:115), AttachBuffers (virtual, vtbl +0x50). Then the stage machine
// (each rung bumps via the @0x82681D30 operator; stages
// eModulePrepareEnvironment / eModuleCreateHierarchyBuilder are EMPTY bumps in
// this build -- asm-verified):
//   Start: bump.
//   Manager: ModuleSingleBuffered::Prepare(); false -> DetachBuffers + false.
//   CreateEnvironment: mEnvironment.Construct({mpAllocator, GetNumberOfStates()}).
//   PrepareProxies: mEnvironment.mModuleParams = akrModuleParams (3 u16 copies
//     -- the module +0x2994.. halfword stores == Environment +0x44).
//   Done: meReleaseStage = eModuleReleaseStart; clear the buffer pointers
//     (the inline detach); return true.
// ---------------------------------------------------------------------------
bool Module::Prepare(rw::IResourceAllocator* apAllocator,
                     CgsModule::IOBuffer* apInputBuffer,
                     CgsModule::IOBuffer* apOutputBuffer,
                     const ModuleParams& akrModuleParams)
{
    CGS_ASSERT(apAllocator != 0, "lpAllocator");
    mpAllocator       = apAllocator;
    gpModuleAllocator = apAllocator;

    mMessageQueue.Prepare();

    CGS_ASSERT(apInputBuffer != 0, "lpInputBuffer");
    CGS_ASSERT(apOutputBuffer != 0, "lpOutputBuffer");
    AttachBuffers(apInputBuffer, apOutputBuffer);

    switch (mePrepareStage)
    {
    case eModulePrepareStart:
        mePrepareStage++;
        // fall through
    case eModulePrepareManager:
        if (!CgsModule::ModuleSingleBuffered::Prepare())
        {
            DetachBuffers();
            return false;
        }
        mePrepareStage++;
        // fall through
    case eModuleCreateEnvironment:
    {
        EnvironmentSpec lSpec;
        lSpec.mpAllocator           = mpAllocator;
        lSpec.mu32StateManagerCount = static_cast<u32>(GetNumberOfStates());
        mEnvironment.Construct(lSpec);
        mePrepareStage++;
        // fall through
    }
    case eModulePrepareEnvironment:
        mePrepareStage++;   // empty stage (console shape)
        // fall through
    case eModuleCreateHierarchyBuilder:
        mePrepareStage++;   // empty stage (console shape)
        // fall through
    case eModulePrepareProxies:
        mEnvironment.mModuleParams.mu16MaxVoiceProxies   = akrModuleParams.mu16MaxVoiceProxies;
        mEnvironment.mModuleParams.mu16MaxContentProxies = akrModuleParams.mu16MaxContentProxies;
        mEnvironment.mModuleParams.mu16MaxStateManagers  = akrModuleParams.mu16MaxStateManagers;
        mePrepareStage++;
        // fall through
    case eModulePrepareDone:
        meReleaseStage      = eModuleReleaseStart;
        mpLogicInputBuffer  = 0;   // the inline detach the Done rung performs
        mpLogicOutputBuffer = 0;
        return true;
    default:
        CGS_ASSERT(false, "Invalid Prepare Stage");
        DetachBuffers();
        return false;
    }
}

// ---------------------------------------------------------------------------
// Module::Update @0x826EAA50  (DWARF :91)
// ---------------------------------------------------------------------------
void Module::Update(f32 af32GameDt, f32 af32SimDt,
                    CgsModule::IOBuffer* apInputBuffer,
                    CgsModule::IOBuffer* apOutputBuffer)
{
    CGS_ASSERT(apInputBuffer != 0, "lpInputBuffer");
    CGS_ASSERT(apOutputBuffer != 0, "lpOutputBuffer");

    AttachBuffers(apInputBuffer, apOutputBuffer);
    mpLogicInputBuffer->LockForRead();
    mpLogicOutputBuffer->LockForWrite();

    ProcessMessageQueue(&mMessageQueue);
    mMessageQueue.Clear();

    mEnvironment.Update(af32GameDt, af32SimDt);

    mpLogicOutputBuffer->UnlockForWrite();
    mpLogicInputBuffer->UnlockForRead();
    DetachBuffers();
}

// ---------------------------------------------------------------------------
// Module::ProcessMessageQueue @0x826C45A8
//
// Walk every queued message, dispatching each to Environment::Notify. The
// console's `event - 4` back-step recovers the message START from the queue's
// returned first-serializable-field pointer (the console record view skips the
// message's leading vtable-pointer word); the host queue's GetFirstEvent
// already returns the record payload start == the Message, so no adjustment.
// ---------------------------------------------------------------------------
void Module::ProcessMessageQueue(CgsModule::VariableEventQueue<8192, 16>* apQueue)
{
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;

    apQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        mEnvironment.Notify(reinterpret_cast<const Io::MessageHeader*>(lpEvent));

        const CgsModule::Event* lpNext = 0;
        apQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
}

// ---------------------------------------------------------------------------
// Module::AttachBuffers @0x8268D3F0 / DetachBuffers @0x8268D400
// ---------------------------------------------------------------------------
void Module::AttachBuffers(CgsModule::IOBuffer* apInputBuffer,
                           CgsModule::IOBuffer* apOutputBuffer)
{
    mpLogicInputBuffer  = apInputBuffer;
    mpLogicOutputBuffer = apOutputBuffer;
}

void Module::DetachBuffers()
{
    mpLogicInputBuffer  = 0;
    mpLogicOutputBuffer = 0;
}

// ---------------------------------------------------------------------------
// Module::GetUniqueId  (DWARF decl; bodied phase B5 -- the trivial member read
// of the id Construct seeds and the owner stamps; the vtable emission of this
// TU demands the symbol.)
// ---------------------------------------------------------------------------
u32 Module::GetUniqueId()
{
    return muUniqueId;
}

// ---------------------------------------------------------------------------
// operator++(Module::EModulePrepareStage&, int)  @ 0x82681D30
//   v1 = *a1; *a1 = v1 + 1; if (v1 + 1 > 6) <assert>; return v1;
// The store of the incremented value happens before the guard, so the increment
// is applied even on the assert path; the return is always the saved old value.
// ---------------------------------------------------------------------------
Module::EModulePrepareStage operator++(Module::EModulePrepareStage& leEnumIndex, int)
{
    const Module::EModulePrepareStage leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<Module::EModulePrepareStage>(static_cast<s32>(leEnumIndex) + 1);

    CGS_ASSERT(leEnumIndex <= Module::eModulePrepareDone,
               "leEnumIndex <= Module::eModulePrepareDone");

    return leOldEnumIndex;
}

// ---------------------------------------------------------------------------
// operator++(Module::EModuleReleaseStage&, int)  (DWARF CgsSoundLogicModule.h:278
// declares it; the Release machine's rungs bump through it -- same shape, bound
// eModuleReleaseDone == 5.)
// ---------------------------------------------------------------------------
Module::EModuleReleaseStage operator++(Module::EModuleReleaseStage& leEnumIndex, int)
{
    const Module::EModuleReleaseStage leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<Module::EModuleReleaseStage>(static_cast<s32>(leEnumIndex) + 1);

    CGS_ASSERT(leEnumIndex <= Module::eModuleReleaseDone,
               "leEnumIndex <= Module::eModuleReleaseDone");

    return leOldEnumIndex;
}

} // namespace Logic
} // namespace CgsSound
