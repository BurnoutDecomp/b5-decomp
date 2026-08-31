#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (attached-buffer guard)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log / Message filter
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // the "Resource Registrar" monitor (Prepare case 0)
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.h"  // Io::LogicPreUpdateOutputBuffer (PreUpdate; phase C1)
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"

// BrnSound::Module::SoundLogicModule -- accessor bodies recovered from
// BURNOUT_X360_ARTIST.XEX. See BrnSoundLogicModule.h for the layout/slice notes.

namespace BrnSound
{
namespace Module
{

namespace
{
    struct GuiEventWireHeader
    {
        u32 muPayloadSize;
        u32 muEventType;
        u32 muPayloadOffset;
    };

    struct GuiAudioEventData
    {
        u8 maData[12];
    };

    const u8* GetGuiPayload(const CgsModule::Event* apEvent, s32 aiEventType,
                            s32 aiEventSize)
    {
        const u8* lpuBytes = reinterpret_cast<const u8*>(apEvent);
        if (aiEventSize >= static_cast<s32>(sizeof(GuiEventWireHeader)))
        {
            const GuiEventWireHeader* lpHeader =
                reinterpret_cast<const GuiEventWireHeader*>(apEvent);
            if (lpHeader->muEventType == static_cast<u32>(aiEventType) &&
                lpHeader->muPayloadOffset >= sizeof(GuiEventWireHeader) &&
                lpHeader->muPayloadOffset < static_cast<u32>(aiEventSize))
            {
                return lpuBytes + lpHeader->muPayloadOffset;
            }
        }
        return lpuBytes;
    }

    template <typename T>
    void QueueSoundMessage(CgsModule::VariableEventQueue<8192, 16>& arQueue,
                           const CgsSound::Io::Message<T>& arMessage)
    {
        arQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&arMessage),
                         arMessage.GetEventId(), static_cast<s32>(sizeof(arMessage)));
    }
}

void SoundLogicModule::ResourcesAreReady()
{
    const bool lbResolved = mBurnoutGlobalData.ResolveLoadedCollection();
    CGS_ASSERT(lbResolved, "mBurnoutGlobalData.IsValid()");
}

// X360 0x826AFF88. Search the per-frame trigger-action table for the entry whose
// EntityId and result-type both match, returning it (or null when absent).
//
// X360 STRUCTURE (0x826AFF88):
//   result = 0; index = 0;
//   base   = &maTriggerActions;           // r30 = this + 0x4CA0
//   do {
//       if (base->miCount == -1) <fire "Array used before Construct/Clear was called">
//       if (index >= base->miCount) break; // unsigned compare against the live count
//       if (base->Ge(index).mEntityId == leEntityId &&
//           base->Ge(index).meResultType == leType)
//           result = &base->Ge(index);
//       ++index;
//   } while (!result);
//   return result;
//
// maTriggerActions.Ge(index) is the committed Array<T,N>::Ge, which carries the
// per-access "Array used before Construct/Clear was called" + bounds asserts the
// X360 body inlines each iteration. EntityId has no operator==, so the identity
// word is compared by its packed value (muValue), matching the X360 raw-word load.
const BrnGameState::GameStateModuleIO::SoundTriggerAction*
SoundLogicModule::GetSoundTriggerAction(
    EntityId leEntityId,
    BrnGameState::GameStateModuleIO::SoundTriggerAction::eType leType)
{
    const BrnGameState::GameStateModuleIO::SoundTriggerAction* lpResult = 0;
    u32 luIndex = 0;
    do
    {
        if (luIndex >= maTriggerActions.GetLength())
        {
            break;
        }
        if (maTriggerActions.Ge(luIndex).mEntityId.muValue == leEntityId.muValue &&
            maTriggerActions.Ge(luIndex).meResultType == leType)
        {
            lpResult = &maTriggerActions.Ge(luIndex);
        }
        ++luIndex;
    }
    while (!lpResult);
    return lpResult;
}

// X360 0x826838C0: `addi r3, r3, 0x588; blr`. Hand out the embedded resource
// registrar by reference (the IResourceRequester override).
BrnSound::Logic::ResourceRegistrar& SoundLogicModule::GetResourceRegistrar()
{
    return mResourceRegistrar;
}

// Bring-up. The X360 ctor (0x827E3DA8) default-constructs the embedded ResourceRegistrar
// (a1+21016); its queues/pools are then initialised by the bring-up Construct (0x826B0470).
// (phase B5): the ENGINE base Construct runs FIRST -- ModuleSingleBuffered + the instance
// counter + the message queue seeds -- then the Brn half: clear the per-frame trigger
// table + Construct the embedded registrar so the broker is live. The 3 Voices (X360
// +20976/+20988/+21000) are still grown on top.
void SoundLogicModule::Construct()
{
    CgsSound::Logic::Module::Construct();

    mpBrnLogicInputBuffer  = 0;
    mpBrnLogicOutputBuffer = 0;

    // The per-frame trigger-action table starts empty (so GetSoundTriggerAction's
    // "used before Construct/Clear" assert is satisfied).
    maTriggerActions.Clear();

    // The streaming-resource broker: bring up its request queues + requested/queued pools.
    mResourceRegistrar.Construct();

    // The pre-update output block's three queues (phase C1; the same trio the
    // RootPreUpdateOutputBuffer carve constructs).
    reinterpret_cast<CgsModule::VariableEventQueue<256, 16>*>(
        mPreUpdateOutput.maGuiOutEventQueueStorage)->Construct();
    mPreUpdateOutput.mAudioCarDataLoadedQueue.Construct();
    mPreUpdateOutput.mAudioEffectsMessageQueue.Construct();

    // The 9 state-manager slots start empty; CreateStateManagers (stage 4) fills them via
    // StateManager::CreateStateMan (null where no leaf is registered). Nulling here keeps
    // PrepareStateManagersOnBoot's `*v5 != 0` guard honest even if a stage runs early.
    for (s32 liIndex = 0; liIndex < KI_NUM_STATE_MANAGERS; ++liIndex)
        mapStateManagers[liIndex] = 0;

    // [grow-in] X360 ctor also constructs the 3 Voices (Submix/Master/GlobalReverb); their
    //   Logic::Voice slice reconciliation is deferred (see Prepare stage 2).

    meBrnPrepareStage = E_PREPSTAGE_PERFMON;
    mbConstructed     = true;

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "[Sound] SoundLogicModule::Construct (registrar live)\n";
}

// X360 0x82703C18 (vtable+0x58). The REAL resumable stage machine (phase B5), console
// switch-for-switch:
//   pre-switch: capture the logic allocator (a1[19777]); assert both buffers (cpp:200/:201);
//     AttachBuffers (the ENGINE virtual, vtbl+0x50).
//   case 0: miResourceRegistrarMonitor = AddMonitor("Resource Registrar", page 14, 2.0) ->
//   case 1: the ENGINE base CgsSound::Logic::Module::Prepare(alloc, in, out, &the 0x820AA480
//     rodata ModuleParams {16,16,16} == the DWARF-declared ModuleParams::DEFAULT); on true
//     advance to 2 (+ the console's progress word = 1); either way DetachBuffers + return
//     false (one chunk per call).
//   case 2: first entry constructs the 3 Voices (Submix ident -16 / Master 1 / GlobalReverb
//     2 against the GenericRwacFactory name + their VoiceSpec names) and returns false; the
//     re-entry Connects Submix+Reverb to "Send01" and LoadAssets BurnoutGlobalData, then
//     falls into
//   case 3: ResourceBridging under the output buffer's write lock -> DetachBuffers + false.
//   case 4: CreateStateManagers ->
//   case 5: LockForWrite(out); PrepareStateManagersOnBoot(4) -- not ready -> ResourceBridging
//     + unlock + detach + false (retry); ready -> unlock ->
//   case 6: DetachBuffers + return true.
//
bool SoundLogicModule::Prepare(rw::IResourceAllocator* apAllocator,
                               CgsModule::IOBuffer* apInputBuffer,
                               CgsModule::IOBuffer* apOutputBuffer)
{
    mpBrnAllocator = apAllocator;   // X360 a1[19777] = a2

    CGS_ASSERT(apInputBuffer, "lpInputBuffer");
    CGS_ASSERT(apOutputBuffer, "lpOutputBuffer");

    // The ENGINE AttachBuffers (X360 vtbl+0x50): pin the engine-side buffer pointers.
    // FLAG (host note): the Brn-side mpBrnLogic* members (+0x4C94/+0x4C98) are pinned
    // beside them -- their console writer is the un-dumped Brn override side; the store
    // keeps GetBrnInputStructure() live exactly as before.
    AttachBuffers(apInputBuffer, apOutputBuffer);
    mpBrnLogicInputBuffer  = reinterpret_cast<Io::LogicInputBuffer*>(apInputBuffer);
    mpBrnLogicOutputBuffer = reinterpret_cast<Io::LogicOutputBuffer*>(apOutputBuffer);

    bool lbPrepared = false;
    switch (meBrnPrepareStage)
    {
    case E_PREPSTAGE_PERFMON:
        // X360 case 0 (a1[19826]): 5-arg AddMonitor, page 14, budget 2.0, libperf-tagged.
        miResourceRegistrarMonitor = CgsDev::PerfMonCpu::AddMonitor(
            "Resource Registrar", static_cast<CgsDev::PerfMonCpuPage>(14), false, 2.0f, true);
        // fall through
    case E_PREPSTAGE_BASE:
        meBrnPrepareStage = E_PREPSTAGE_BASE;
        // The ENGINE base's resumable Prepare (base + environment + proxies stages).
        if (CgsSound::Logic::Module::Prepare(apAllocator, apInputBuffer, apOutputBuffer,
                                             CgsSound::Logic::ModuleParams::DEFAULT))
        {
            meBrnPrepareStage = E_PREPSTAGE_VOICES;
        }
        break;   // one chunk per call (console LABEL_20: detach + return 0)
    case E_PREPSTAGE_VOICES:
        meBrnPrepareStage = E_PREPSTAGE_VOICES;
        if (!mMasterVoice.GetVoiceObject())
        {
            const u32 luFactoryName = static_cast<u32>(
                CgsSound::Playback::GenericRwacFactorySkName().GetValue());
            mSubmixVoice.Construct(
                this, CgsSound::Playback::KU_INIT_SND9_SUBMIX_IDENT, luFactoryName,
                static_cast<u32>(CgsSound::Playback::Name::MakeHash("SubmixVoiceSpec")));
            mMasterVoice.Construct(
                this, 1, luFactoryName,
                static_cast<u32>(CgsSound::Playback::Name::MakeHash("MasterVoiceSpec")));
            mGlobalReverbVoice.Construct(
                this, 2, luFactoryName,
                static_cast<u32>(CgsSound::Playback::Name::MakeHash("GlobalReverbVoiceSpec")));
            break; // console returns after the construct pass and re-enters stage 2.
        }
        {
            const u32 luSend01 = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("Send01"));
            mSubmixVoice.Connect(luSend01, 1);
            mGlobalReverbVoice.Connect(luSend01, 1);
            LoadAsset("Sound\\BurnoutGlobalData.bin", "BurnoutGlobalData",
                      BrnSound::Logic::ResourceRegistrar::E_ATTRIBSYS);
        }
        // fall through
    case E_PREPSTAGE_BRIDGE:
        meBrnPrepareStage = E_PREPSTAGE_BRIDGE;
        // X360 case 3 / LABEL_12: ResourceBridging under the output write lock.
        apOutputBuffer->LockForWrite();
        ResourceBridging();
        apOutputBuffer->UnlockForWrite();
        meBrnPrepareStage = E_PREPSTAGE_STATEMANAGERS;
        break;   // console: detach + return 0 after the bridge chunk
    case E_PREPSTAGE_STATEMANAGERS:
        meBrnPrepareStage = E_PREPSTAGE_STATEMANAGERS;
        // X360 case 4: create the 9 managers + register them into the ENGINE base's
        // Environment.
        CreateStateManagers();
        // fall through
    case E_PREPSTAGE_BOOTPREPARE:
        meBrnPrepareStage = E_PREPSTAGE_BOOTPREPARE;
        // X360 case 5, under the output write lock; a not-ready manager bridges +
        // retries next tick (stage stays here).
        apOutputBuffer->LockForWrite();
        if (!PrepareStateManagersOnBoot(4))
        {
            ResourceBridging();              // console LABEL_13 on the retry path
            apOutputBuffer->UnlockForWrite();
            break;
        }
        apOutputBuffer->UnlockForWrite();
        meBrnPrepareStage = E_PREPSTAGE_DONE;
        // fall through
    case E_PREPSTAGE_DONE:
        meBrnPrepareStage = E_PREPSTAGE_DONE;
        lbPrepared = true;
        break;
    default:
        CGS_ASSERT(false, "Invalid Stage\n");
        break;
    }

    // Console: every exit detaches the per-call buffers (vtbl+0x54).
    DetachBuffers();

    if (lbPrepared && !mbPrepared)
    {
        mbPrepared = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "[Sound] SoundLogicModule::Prepare: stage machine "
                                          "complete (engine base + environment live; voices "
                                          "grow-in) -> prepared\n";
    }
    return lbPrepared;
}

// X360 0x82702E80. Bridge the broker's per-frame resource traffic:
//   ResourceRegistrar::Update(this+21016);                                    // <- real, runs now
//   VariableEventQueue<4096,16>::Append(*(this+19608)+2068, this+74952);      // <- grow-in
//   VariableEventQueue<2048,16>::Append(*(this+19608)+4,    this+72888);      // <- grow-in
// MINIMAL-THEN-GROW: the registrar Update is REAL (drains the request queues, resolves requested
// resources to handles, promotes queued->requested, GCs unreferenced files). On the freshly-
// Construct'd empty registrar at boot it is a safe no-op (empty queues/pools), but this is what
// actually exercises the reconstructed broker Update path at runtime.
void SoundLogicModule::ResourceBridging()
{
    mResourceRegistrar.Update();
    CGS_ASSERT(mpBrnLogicOutputBuffer != 0, "mpBrnLogicOutputBuffer");
    mpBrnLogicOutputBuffer->GetResourceRequestInterface()->mRequestQueue.Append(
        mResourceRegistrar.GetResourceRequestInterface().mRequestQueue);
    mpBrnLogicOutputBuffer->GetAttribSysRequestInterface()->mRequestQueue.Append(
        mResourceRegistrar.GetAttribSysRequestInterface().mRequestQueue);
}

void SoundLogicModule::ProcessGuiEvents(
    const CgsModule::VariableEventQueue<18432, 16>* apGuiEvents)
{
    if (!apGuiEvents)
        return;

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = apGuiEvents->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        const u8* lpuPayload = GetGuiPayload(lpEvent, liType, liSize);
        switch (liType)
        {
        case 23:
        {
            const CgsGui::GuiEventPlayMusicOnMenuStream* lpGuiEvent =
                reinterpret_cast<const CgsGui::GuiEventPlayMusicOnMenuStream*>(lpEvent);
            CgsSound::Io::Message<CgsGui::GuiEventPlayMusicOnMenuStream> lMessage(*lpGuiEvent);
            lMessage.Construct(13, 0, 0, 2, CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
            QueueSoundMessage(mMessageQueue, lMessage);
            break;
        }
        case 201: // PC's typed GuiAudioTriggerEvent id
        case 457: // ARTIST wire id
        {
            CgsSound::Io::Message<BrnGui::GuiAudioTriggerEvent> lMessage;
            lMessage.Construct(6, 0, 0, 0, CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
            if (liType == 201 && liSize >= static_cast<s32>(sizeof(lMessage.mData)))
            {
                lMessage.mData =
                    *reinterpret_cast<const BrnGui::GuiAudioTriggerEvent*>(lpEvent);
            }
            else
            {
                const BrnGui::GuiAudioTriggerWirePayload457& lrPayload =
                    *reinterpret_cast<const BrnGui::GuiAudioTriggerWirePayload457*>(
                        lpuPayload);
                lMessage.mData.Construct(
                    lrPayload.meAction, lrPayload.macComponent,
                    lrPayload.macLabel, lrPayload.macMovie);
            }
            QueueSoundMessage(mMessageQueue, lMessage);
            break;
        }
        case 456:
        {
            CgsSound::Io::Message<GuiAudioEventData> lMessage;
            lMessage.Construct(5, 0, 0, 1, CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
            std::memcpy(lMessage.mData.maData, lpuPayload, sizeof(lMessage.mData.maData));
            QueueSoundMessage(mMessageQueue, lMessage);
            break;
        }
        case 466:
        {
            const u32 luName = *reinterpret_cast<const u32*>(lpuPayload);
            CgsSound::Io::Message<CgsSound::Playback::Name> lMessage;
            lMessage.Construct(36, 0, 0, 5, CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
            lMessage.mData = CgsSound::Playback::Name(static_cast<uintptr_t>(luName));
            QueueSoundMessage(mMessageQueue, lMessage);
            break;
        }
        case 468:
        {
            const u32 luName = *reinterpret_cast<const u32*>(lpuPayload);
            CgsSound::Io::Message<CgsSound::Playback::Name> lMessage;
            lMessage.Construct(28, 0, 0, 2, CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
            lMessage.mData = CgsSound::Playback::Name(static_cast<uintptr_t>(luName));
            QueueSoundMessage(mMessageQueue, lMessage);
            break;
        }
        case 469:
        {
            CgsSound::Io::Message<bool> lMessage;
            lMessage.Construct(44, 0, 0, 1, CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
            lMessage.mData = (*lpuPayload != 0);
            QueueSoundMessage(mMessageQueue, lMessage);
            break;
        }
        default:
            break;
        }

        liType = apGuiEvents->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}

// ARTIST 0x826EC250. The playback module reports the voice id for every stream
// buffer that has completed its close/grace-period cycle. StreamingEffect::Detach
// waits for this message before it releases its State, so each id is addressed to
// effect object 0 in all three StreamingState instances (manager 6).
void SoundLogicModule::ProcessStreamFreedQueue(
    const CgsSound::Playback::Module::Io::OutputBuffer::FreedBuffersArray& arFreedIds)
{
    for (u32 luFreed = 0; luFreed < arFreedIds.GetLength(); ++luFreed)
    {
        for (u16 luInstance = 0; luInstance < 3; ++luInstance)
        {
            CgsSound::Io::Message<CgsSound::Io::QueueElement> lMessage;
            lMessage.Construct(16, 6, luInstance, 0,
                               CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
            lMessage.mData = arFreedIds.GetItem(luFreed);
            QueueSoundMessage(mMessageQueue, lMessage);
        }
    }
}

void SoundLogicModule::ProcessCarDataLoadingQueue(
    const Io::AudioCarLoadedDataQueue& arEvents)
{
    using BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent;
    for (s32 liEvent = 0; liEvent < arEvents.GetLength(); ++liEvent)
    {
        const AudioCarDataLoadedEvent& lrEvent = arEvents.GetEvent(liEvent);
        CGS_ASSERT(lrEvent.meMessageType == AudioCarDataLoadedEvent::E_REQUEST_LOAD_DATA ||
                   lrEvent.meMessageType == AudioCarDataLoadedEvent::E_REQUEST_UNLOAD_DATA,
                   "lAudioCarDataLoadedEvent.GetMessageType() == AudioCarDataLoadedEvent::E_REQUEST_LOAD_DATA || lAudioCarDataLoadedEvent.GetMessageType() == AudioCarDataLoadedEvent::E_REQUEST_UNLOAD_DATA");
        if (lrEvent.meMessageType == AudioCarDataLoadedEvent::E_REQUEST_LOAD_DATA)
        {
            BrnSound::Vehicles::VehicleStateManager::AddEntry(
                lrEvent.mAssetID, lrEvent.mpVehicleListEntry,
                lrEvent.miActiveRaceCarIndex, lrEvent.mbIsPlayer);
        }
        else if (lrEvent.meMessageType == AudioCarDataLoadedEvent::E_REQUEST_UNLOAD_DATA)
        {
            BrnSound::Vehicles::VehicleStateManager::RemoveEntry(
                lrEvent.mAssetID, lrEvent.miActiveRaceCarIndex);
        }
    }
}

// ARTIST 0x826978A0 / 0x826978B8. SoundLogicModule mirrors the generic logic
// engine's buffer pair into its typed Burnout pair. State managers use the typed
// input during Environment::Update, so both pairs must have the same lifetime.
void SoundLogicModule::AttachBuffers(CgsModule::IOBuffer* apInputBuffer,
                                     CgsModule::IOBuffer* apOutputBuffer)
{
    CgsSound::Logic::Module::AttachBuffers(apInputBuffer, apOutputBuffer);
    mpBrnLogicInputBuffer = static_cast<Io::LogicInputBuffer*>(apInputBuffer);
    mpBrnLogicOutputBuffer = static_cast<Io::LogicOutputBuffer*>(apOutputBuffer);
}

void SoundLogicModule::DetachBuffers()
{
    CgsSound::Logic::Module::DetachBuffers();
    mpBrnLogicInputBuffer = 0;
    mpBrnLogicOutputBuffer = 0;
}

// ARTIST 0x826C9860. Camera microphone 0 receives the director camera matrix.
// When the player car is active, player microphone 0 uses the same orientation
// at the player-car position. The original KVF_CAR_MIC_OFFSET at 0x830060B0 is
// a zero broadcast in ARTIST, so the flattened/normalised camera-at offset term
// evaluates to zero while retaining the source operation's structure here.
void SoundLogicModule::UpdateMicrophones(const Io::LogicInputBuffer* apLogicInputBuffer)
{
    const Io::RootInputBuffer::DirectorCamera* lpCamera =
        apLogicInputBuffer->GetDirectorCamera();
    CgsSound::Logic::MicrophoneSystem& lrMicrophones =
        GetEnvironment().GetMicrophoneSystem();

    const rw::math::vpu::Matrix44Affine lCameraTransform = lpCamera->GetTransform();
    lrMicrophones.GetMicrophone(CgsSound::Logic::MicrophoneSystem::E_MIC_CAMERA,
                               CgsSound::Logic::MicrophoneSystem::E_PLAYER_1)
        ->SetMicrophoneMatrix(lCameraTransform);

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        lpVehicles = apLogicInputBuffer->GetVehicleInterface();
    if (lpVehicles->IsPlayerCarActive())
    {
        const EActiveRaceCarIndex lePlayerIndex =
            apLogicInputBuffer->GetPlayerActiveRaceCarIndex();
        const BrnPhysics::Vehicle::RaceCarState* lpPlayerVehicle =
            lpVehicles->GetRaceCarState(lePlayerIndex);

        rw::math::vpu::Vector3 lvCameraAt = lCameraTransform.At();
        lvCameraAt.y = 0.0f;
        lvCameraAt = rw::math::vpu::Normalize(lvCameraAt);

        const rw::math::vpu::Vector3 lvNewCarPos =
            lpPlayerVehicle->mTransform.Pos() - (lvCameraAt * 0.0f);
        rw::math::vpu::Matrix44Affine lCarTransform = lCameraTransform;
        lCarTransform.Pos() = lvNewCarPos;
        lrMicrophones.GetMicrophone(CgsSound::Logic::MicrophoneSystem::E_MIC_PLAYER,
                                   CgsSound::Logic::MicrophoneSystem::E_PLAYER_1)
            ->SetMicrophoneMatrix(lCarTransform);
    }

    lrMicrophones.SetNumberOfPlayers(1);
}

// ARTIST 0x826B0040. Build the per-frame player/camera snapshot consumed by
// collision, world-emitter, passby, and global mixer logic.
void SoundLogicModule::UpdateFrameInformation(
    f32 af32SimDt,
    const Io::LogicInputBuffer* apLogicInputBuffer,
    BrnUpdateSet aeUpdateSet,
    EActiveRaceCarIndex aePlayerCarIndex)
{
    static const f32 KF_IMPACT_TIME_THRESHOLD = 0.0033333336f;
    static const f32 KF_SLOW_MO_THRESHOLD = 0.012500001f;

    const f32 lfSimTimeScale =
        apLogicInputBuffer->GetDirectorCamera()->GetEffects().GetSimTimeScale();
    const bool lbImpactTime =
        af32SimDt < KF_IMPACT_TIME_THRESHOLD && lfSimTimeScale < 1.0f;
    const bool lbSlowMo =
        af32SimDt < KF_SLOW_MO_THRESHOLD && lfSimTimeScale < 1.0f;

    mFrameInformation.meImpactTime.Update(
        lbImpactTime ? AttribSys::Enums::eImpactTime::VSlow
                     : (lbSlowMo ? AttribSys::Enums::eImpactTime::True
                                 : AttribSys::Enums::eImpactTime::False));

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        lpVehicles = apLogicInputBuffer->GetVehicleInterface();
    mFrameInformation.mIsHardStop.Update(lpVehicles->IsPlayerCarCrashing());
    mFrameInformation.mbInReplay = (aeUpdateSet & 0x0100u) != 0;

    if (lpVehicles->IsPlayerCarActive())
    {
        const BrnPhysics::Vehicle::RaceCarState* lpPlayerVehicle =
            lpVehicles->GetRaceCarState(aePlayerCarIndex);
        mFrameInformation.UpdateFatalityFlag(lpVehicles->IsPlayerCarFatalyCrashing());
        mFrameInformation.mPlayerTransform = lpPlayerVehicle->mTransform;
    }
}

void SoundLogicModule::Update(f32 af32GameDt, f32 af32SimDt,
                              CgsModule::IOBuffer* apInputBuffer,
                              CgsModule::IOBuffer* apOutputBuffer)
{
    Update(af32GameDt, af32SimDt, apInputBuffer, apOutputBuffer,
           static_cast<BrnUpdateSet>(0));
}

void SoundLogicModule::Update(f32 af32GameDt, f32 af32SimDt,
                              CgsModule::IOBuffer* apInputBuffer,
                              CgsModule::IOBuffer* apOutputBuffer,
                              BrnUpdateSet aeUpdateSet)
{
    CGS_ASSERT(apInputBuffer != 0, "lpInputBuffer");
    CGS_ASSERT(apOutputBuffer != 0, "lpOutputBuffer");

    AttachBuffers(apInputBuffer, apOutputBuffer);

    // ARTIST 0x82702A78..0x82702A90 starts every logic tick by emptying the
    // per-frame outputs: maTriggerActions' count @+0x4EA0, the GuiOut queue
    // @+0x4EB0, the car-data queue count @+0x4FC8, and the audio-effects queue
    // @+0x5150.  PreUpdate publishes this block before the next Update, so
    // clearing it here gives each event exactly one frame of lifetime.
    maTriggerActions.Clear();
    reinterpret_cast<CgsModule::VariableEventQueue<256, 16>*>(
        mPreUpdateOutput.maGuiOutEventQueueStorage)->Clear();
    mPreUpdateOutput.mAudioCarDataLoadedQueue.Clear();
    mPreUpdateOutput.mAudioEffectsMessageQueue.Clear();

    Io::RootInputBuffer* lpInput = static_cast<Io::RootInputBuffer*>(apInputBuffer);
    lpInput->LockForRead();
    UpdateMicrophones(lpInput);
    const EActiveRaceCarIndex lePlayerCarIndex = lpInput->GetPlayerActiveRaceCarIndex();
    UpdateFrameInformation(af32SimDt, lpInput, aeUpdateSet, lePlayerCarIndex);
    const Io::RootInputBuffer::GuiEventQueue* lpGuiQueue = lpInput->GetGuiEventQueue();
    ProcessGuiEvents(reinterpret_cast<const CgsModule::VariableEventQueue<18432, 16>*>(
        lpGuiQueue));
    ProcessCarDataLoadingQueue(*lpInput->GetAudioCarDataLoadedQueueForRead());
    lpInput->UnlockForRead();

    CgsSound::Logic::Module::Update(af32GameDt, af32SimDt, apInputBuffer, apOutputBuffer);

    // ARTIST 0x82702D8C..0x82702D98: publish playback's freed stream
    // identifiers into the logic message queue, then consume the list. The
    // messages are intentionally processed by the base Update on the next tick.
    ProcessStreamFreedQueue(mFreedStreamBufferIds);
    mFreedStreamBufferIds.Clear();

    // The resource broker must continue to drain after boot; effect and manager
    // LoadAsset requests are resolved through this pass. X360 0x82702E14..0x82702E58
    // brackets the final request-queue append with output-then-input write locks and
    // releases them in the same order. ResourceBridging is the identical append pair
    // factored by Prepare, with the registrar Update immediately ahead of it.
    AttachBuffers(apInputBuffer, apOutputBuffer);
    mpBrnLogicOutputBuffer->LockForWrite();
    mpBrnLogicInputBuffer->LockForWrite();
    ResourceBridging();
    mpBrnLogicOutputBuffer->UnlockForWrite();
    mpBrnLogicInputBuffer->UnlockForWrite();
    DetachBuffers();
}

// X360 0x826AFEF8. Create the 9 sound-logic state managers and register them in the
// embedded Environment. X360 store-for-store (a1 == this):
//   v2 = a1 + 10576;            ; &lEnvironment
//   v3 = 0;  v4 = a1 + 79064;   ; slot index + &mapStateManagers[0]
//   do {
//       result = StateManager::CreateStateMan(v3, a1);   ; factory(i, this)
//       *v4 = result;                                    ; mapStateManagers[i] = result
//       if ( result ) {
//           result = Environment::AddStateManager(v2);   ; lEnvironment.AddStateManager(result)
//           if ( !result ) <assert "lEnvironment.AddStateManager( mapStateManagers[ i ] )"  // :753>
//       }
//       ++v3; ++v4;
//   } while ( v3 < 9 );
//
// Reproduced BY NAME: the loop fills mapStateManagers[i] from the factory (which scans
// the RTTI registry by id), and every non-null manager is registered in lEnvironment.
// The X360 guards the AddStateManager call with `if (result)` -- a null slot (no leaf
// registered for that id) is skipped. NOTE (2026-08-25): the 8 manager TUs ARE in the build, so the
// registry is empty, every CreateStateMan returns null, every slot is set null, and
// AddStateManager is never called -> a safe no-op exactly as the X360 degrades.
//
// FLAG (faithful guard): the X360 asserts the AddStateManager *return* (it returns 1 on
// every path -- see CgsEnvironment.cpp -- so the assert is a vacuous tripwire). The
// embedded Environment's AddStateManager itself asserts the manager is non-null, its
// state-type is in range, and the slot is free; those are the real registration guards.
void SoundLogicModule::CreateStateManagers()
{
    for (s32 liIndex = 0; liIndex < KI_NUM_STATE_MANAGERS; ++liIndex)
    {
        mapStateManagers[liIndex] =
            CgsSound::Logic::StateManager::CreateStateMan(static_cast<u32>(liIndex), this);

        if (mapStateManagers[liIndex] != 0)
        {
            // The X360 asserts this returns true (it always does); kept as the faithful
            // registration call. AddStateManager itself fires the real registration asserts.
            // (phase B5: the environment is the ENGINE base's, via GetEnvironment().)
            bool lbRegistered = GetEnvironment().AddStateManager(mapStateManagers[liIndex]);
            CGS_ASSERT(lbRegistered,
                       "lEnvironment.AddStateManager( mapStateManagers[ i ] )");
            (void)lbRegistered;
        }
    }
}

// X360 0x826837F8. Boot-prepare the created state managers (boot caller passes mask 4).
// X360 store-for-store (a1 == this, a2 == luSkipMask):
//   v3 = a1 + 79064; v4 = 0; v5 = a1 + 79064;          ; &mapStateManagers[0]
//   do {
//       if ( ((1 << v4) & a2) == 0 && *v5 && !(*(**v5 + 12))(*v5) )   ; skip-bit / null-guard / Prepare()
//           return 0;                                                  ; a manager not ready -> retry boot
//       ++v4; ++v5;
//   } while ( v4 < 9 );
//   if ( *v3 ) {                                          ; mapStateManagers[0]
//       v6 = (*(**v3 + 20))(*v3, 0);                      ; GetChildStateManager(0)  (vtable +0x14)
//       if ( v6 ) (*(*v6 + 12))(v6, 0);                   ; child->Prepare()         (vtable +0x0C)
//   }
//   return 1;
//
// Reproduced BY NAME: for each slot not masked out and non-null, call its Prepare()
// (the per-manager bring-up state machine, overridden by each leaf); a false return
// aborts (returns false so the boot stage stays and retries). Then the
// mapStateManagers[0] child special-case: fetch its child via GetChildStateManager(0)
// and, if present, Prepare() the child. The null-guard `*v5 != 0` means empty slots
// (no registered leaf) are skipped -> (2026-08-25: managers ARE in the build now; only truly-empty ids return)
// true immediately (safe no-op), matching the X360's degenerate behaviour.
//
// NOTE (skip-mask sense): the X360 SKIPS Prepare when `((1<<i) & mask) != 0`; mask 4 ==
// bit 2, so on boot slot 2's Prepare is skipped here (faithful to the boot call).
bool SoundLogicModule::PrepareStateManagersOnBoot(s32 luSkipMask)
{
    for (s32 liIndex = 0; liIndex < KI_NUM_STATE_MANAGERS; ++liIndex)
    {
        const bool lbSkip = (((1 << liIndex) & luSkipMask) != 0);
        if (!lbSkip && mapStateManagers[liIndex] != 0)
        {
            if (!mapStateManagers[liIndex]->Prepare())   // vtable +0x0C
            {
                return false;   // not ready yet -> the boot stage retries
            }
        }
    }

    // The mapStateManagers[0] global-state attach special-case. The vtable +0x14
    // call is StateManager::GetFreeState(void*), and the returned State's +0x0C
    // slot is State::Attach(void*) -- not a child-manager Prepare call.
    if (mapStateManagers[0] != 0)
    {
        CgsSound::Logic::State* lpGlobalState =
            mapStateManagers[0]->GetFreeState(0);           // vtable +0x14
        if (lpGlobalState != 0)
            lpGlobalState->Attach(0);                       // State vtable +0x0C
    }

    return true;
}

// X360 0x82682518. Return the attached sound logic input buffer, asserting it is
// non-null first (the X360 fires CgsDev::Assert with the stringized member name
// "mpBrnLogicInputBuffer" at BrnSoundLogicModule.h:432, then still returns the
// pointer -- a non-gating tripwire).
Io::LogicInputBuffer* SoundLogicModule::GetBrnInputStructure()
{
    CGS_ASSERT(mpBrnLogicInputBuffer, "mpBrnLogicInputBuffer");
    return mpBrnLogicInputBuffer;
}

// X360 0x826E1F10 (DWARF :152; bodied 2026-08-25, faithful-audio-engine phase C1).
// Publish the module's accumulated pre-update output block into the caller's
// scratch buffer: assert (cpp:495), write-lock, the SetPreUpdateOutput copy
// (@0x826E0C10 -- the two memcpy spans + the car-data Clear+Append), unlock.
void SoundLogicModule::PreUpdate(Io::LogicPreUpdateOutputBuffer* apLogicPreUpdateOutput)
{
    CGS_ASSERT(apLogicPreUpdateOutput != 0, "lpLogicPreUpdateOutput");
    apLogicPreUpdateOutput->LockForWrite();
    apLogicPreUpdateOutput->SetPreUpdateOutput(mPreUpdateOutput);
    apLogicPreUpdateOutput->UnlockForWrite();
}

} // namespace Module
} // namespace BrnSound
