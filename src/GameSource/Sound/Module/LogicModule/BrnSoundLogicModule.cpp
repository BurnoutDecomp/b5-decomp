#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (attached-buffer guard)
#include <cstdlib>   // std::getenv (the [music] GUI-33 witness)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log / Message filter
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // the "Resource Registrar" monitor (Prepare case 0)
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.h"  // Io::LogicPreUpdateOutputBuffer (PreUpdate; phase C1)
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
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

    // The wire record of GUI event 456 (BrnGui::GuiAudioEvent: s32 component type, s32 action,
    // s32 additional information, CgsID hud message id) is 24 bytes -- ARTIST case 456
    // @0x826EE0C8..0x826EE108 copies it with three doubleword stores. 12 truncated the CgsID
    // HUDEffect::FindEventMapping keys on, so no HUD sting could ever match a mapping.
    struct GuiAudioEventData
    {
        u8 maData[24];
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

    // Construct + queue in one statement, the way the X360 dispatch folds it (every
    // case builds the 16-byte header on the stack, drops the payload at +0x10 and
    // calls VariableEventQueue<8192,16>::AddEvent with the event id).
    template <typename T>
    void PostSoundMessage(CgsModule::VariableEventQueue<8192, 16>& arQueue,
                          s16 ai16EventId, u16 au16StateManagerId, u16 au16InstanceId,
                          u16 au16EffectId,
                          CgsSound::Io::MessageHeader::eEffectTypes aeEffectType,
                          const T& arPayload)
    {
        CgsSound::Io::Message<T> lMessage;
        lMessage.Construct(ai16EventId, au16StateManagerId, au16InstanceId,
                           au16EffectId, aeEffectType);
        lMessage.mData = arPayload;
        QueueSoundMessage(arQueue, lMessage);
    }

    // Size-attested opaque payloads (house style: the record's SIZE is X360-attested by
    // the AddEvent arg / the memcpy width; the internal field names are not recovered
    // here, and the consumer reads them at the console's own byte offsets).
    struct GuiAudioTraxUpdatePayload   { u8 maData[32]; };  // sound message 7  (GUI 458)
    struct GuiAudioTraxIndexesPayload  { u8 maData[24]; };  // sound message 8  (GUI 459)
    struct GuiAudioTraxPreviewPayload  { u8 maData[8];  };  // sound message 9  (GUI 460)
    struct GuiAudioEventIntrosPayload  { u8 maData[24]; };  // sound message 31 (GUI 464)
    struct GuiAudioSettingsPayload     { u8 maData[8];  };  // sound message 12 (GUI 463): {music, sfx}
    struct ShowModeResultsPayload      { u8 maData[232]; }; // sound message 23 (action 37)
    struct RoadRageDamagePayload       { u8 maData[8];  };  // sound message 20 (action 205)

    // Read a 4-byte field out of an event record at the console's own byte offset.
    s32 EventS32At(const CgsModule::Event* apEvent, u32 auOffset)
    {
        s32 liValue = 0;
        std::memcpy(&liValue, reinterpret_cast<const u8*>(apEvent) + auOffset,
                    sizeof(liValue));
        return liValue;
    }
    u8 EventU8At(const CgsModule::Event* apEvent, u32 auOffset)
    {
        return reinterpret_cast<const u8*>(apEvent)[auOffset];
    }
    u64 EventU64At(const CgsModule::Event* apEvent, u32 auOffset)
    {
        u64 lu64Value = 0;
        std::memcpy(&lu64Value, reinterpret_cast<const u8*>(apEvent) + auOffset,
                    sizeof(lu64Value));
        return lu64Value;
    }

    // X360 unk_82F2FD38: eleven 8-byte records whose SECOND word is the key game action
    // 56 matches its own +4 field against (read with tools/re/x360rd.py). A hit posts the
    // stunt-jump FX message.
    const u32 KA_STUNT_JUMP_KEYS[11] =
    {
        0x00075542u, 0x00075540u, 0x00075541u, 0x000782CDu, 0x000782CEu, 0x000782A8u,
        0x0007725Au, 0x00077255u, 0x0007FC80u, 0x0004E38Cu, 0x0007B999u
    };
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

// ---------------------------------------------------------------------------
// SoundLogicModule::GetUniqueId  @ 0x826838C8   (override of the engine's
// CgsSound::Logic::Module::GetUniqueId @0x827E1078)
//
//   mr    r8, r3
//   li    r9, -0x14                     ; 0xFFFFFFEC
//   lwz   r11, 0x5214(r8)               ; muBrnUniqueId  (NOT the base's +0x230)
// loop:
//   addi  r3, r11, 1                    ; candidate = cursor + 1
//   addi  r10, r3, -3                   ; candidate - 3
//   mr    r11, r3
//   subfc r10, r10, r9                  ; CA = (0xFFFFFFEC >= candidate-3) unsigned
//   subfe r10, r10, r10                 ; 0 when CA, -1 otherwise
//   clrlwi r10, r10, 31
//   bne   cr6, loop                     ; loop while candidate-3 > 0xFFFFFFEC
//   stw   r3, 0x5214(r8)
//
// i.e. hand out the next id, SKIPPING every candidate for which
// (u32)(candidate - 3) > 0xFFFFFFEC -- that is candidate in
// [0xFFFFFFF0 .. 0xFFFFFFFF] U {0, 1, 2}: the 19 RESERVED idents.  The reserve
// set is exactly the idents this module's own three fixed voices carry
// (mMasterVoice 1, mGlobalReverbVoice 2, mSubmixVoice -16 == 0xFFFFFFF0 ==
// Playback::KU_INIT_SND9_SUBMIX_IDENT, which Playback::Module::CreateVoice
// @0x826D7B00 tests by value), so no generated id can ever alias one of them.
// From a zero cursor the first id handed out is 3.
// ---------------------------------------------------------------------------
u32 SoundLogicModule::GetUniqueId()
{
    u32 luCandidate = muBrnUniqueId;
    do
    {
        ++luCandidate;
    }
    while ((luCandidate - 3u) > 0xFFFFFFECu);

    muBrnUniqueId = luCandidate;
    return luCandidate;
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

    mRandomGenerator.Construct();

    mpBrnLogicInputBuffer  = 0;
    mpBrnLogicOutputBuffer = 0;

    // The module's own unique-id cursor (X360 this+0x5214). The console's ctor
    // @0x827E3DA8 does not write this word -- it relies on the module block being
    // zero at allocation -- and its generator @0x826838C8 skips 0/1/2, so the first
    // id it hands out is 3. Seeding 0 here reproduces that exactly; on the host the
    // storage is not guaranteed zero, so the seed is explicit.
    muBrnUniqueId = 0;

    // The per-frame trigger-action table starts empty (so GetSoundTriggerAction's
    // "used before Construct/Clear" assert is satisfied).
    maTriggerActions.Clear();

    // The dispatch state block (X360 this+0x13570) starts zeroed.
    std::memset(&mDispatchState, 0, sizeof(mDispatchState));

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
            // X360 0x826ED6C8 case 23 copies the wire record's three fields into the
            // message payload at +0x00 / +0x04 / +0x05 -- NOT the wire record itself
            // (which carries a 12-byte GuiEvent<23> header here). See
            // BrnSound::MusicOnMenuStreamData.
            const CgsGui::GuiEventPlayMusicOnMenuStream* lpGuiEvent =
                reinterpret_cast<const CgsGui::GuiEventPlayMusicOnMenuStream*>(lpEvent);
            BrnSound::MusicOnMenuStreamData lData;
            lData.muStreamNameHash = lpGuiEvent->muStreamNameHash;
            lData.mbFromVideo = lpGuiEvent->mbFlagA ? 1u : 0u;
            lData.mbPlayOverCustomSoundtrack = lpGuiEvent->mbFlagB ? 1u : 0u;
            lData.maReserved[0] = 0;
            lData.maReserved[1] = 0;
            PostSoundMessage(mMessageQueue, 13, 0, 0, 2,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT, lData);
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
            // X360 0x826ED6C8 case 469: BOTH the 100%-complete flag AND the menu stream
            // that goes with it. The stream is "Guns_And_Roses" when the flag is set and
            // K_NULL_NAME (0, read from dword_83008228) when it is clear -- the string
            // pointer is the .rdata word at 0x82F2CE5C. Effect type is CONTROL (2) for
            // the flag message; the tree previously used OBJECT and dropped the second
            // message entirely.
            const bool lbHundredPercent = (*lpuPayload != 0);
            CgsSound::Io::Message<bool> lMessage;
            lMessage.Construct(44, 0, 0, 1, CgsSound::Io::MessageHeader::E_EFFECT_TYPE_CONTROL);
            lMessage.mData = lbHundredPercent;
            QueueSoundMessage(mMessageQueue, lMessage);

            BrnSound::MusicOnMenuStreamData lMenu;
            lMenu.muStreamNameHash =
                lbHundredPercent
                    ? static_cast<u32>(CgsSound::Playback::Name::MakeHash("Guns_And_Roses"))
                    : 0u;
            lMenu.mbFromVideo = 1;
            lMenu.mbPlayOverCustomSoundtrack = 0;
            lMenu.maReserved[0] = 0;
            lMenu.maReserved[1] = 0;
            PostSoundMessage(mMessageQueue, 13, 0, 0, 2,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT, lMenu);
            break;
        }

        // ---- the twelve cases this function used to drop ---------------------------
        case 31:  // GuiEventSetLanguage -- to the speech effect AND the music effect.
        {
            CGS_ASSERT(lpEvent != 0, "lpLanguageEvent");
            const s32 leLanguage = EventS32At(lpEvent, 0);
            PostSoundMessage(mMessageQueue, 33, 0, 0, 5,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT, leLanguage);
            PostSoundMessage(mMessageQueue, 33, 0, 0, 2,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT, leLanguage);
            break;
        }

        case 33:  // GuiEventLoadingScreenState -- bit 1 of the dispatch flags word
                  // (X360 @0x826EDCCC: `ld` / `ori 2` or `clrrdi`-style clear / `std`).
            CGS_ASSERT(lpEvent != 0, "lpLoadingScreenEvent");
            if (EventU8At(lpEvent, 0))
                mDispatchState.mu64Flags |= 2u;
            else
                mDispatchState.mu64Flags &= ~static_cast<u64>(2u);
            // [DIAG] NOT IN THE X360 BINARY -- the loading-screen witness the [music] lines
            // were missing: this bit is half of MusicEffect::UpdateParams' stream-pause gate.
            if (std::getenv("BRN_MUSIC_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint
                    << "[music] GUI 33 loading screen visible=" << static_cast<s32>(EventU8At(lpEvent, 0))
                    << " -> dispatch flags 0x" << static_cast<s32>(mDispatchState.mu64Flags & 0xFF)
                    << " simPausedBit " << static_cast<s32>(mDispatchState.mu8SimPausedBit) << "\n";
            break;

        case 88:  // Stop / start the playback DAC outright. X360:
                  //   lpEnv = CgsSound::Playback::Module::Module::GetEnvironment(this+568);
                  //   *payload ? Playback::Environment::StopDac(lpEnv)
                  //            : Playback::Environment::StartDac(lpEnv);
                  // The PLAYBACK module the logic module reaches at +0x238 has no named
                  // accessor in this tree (GetEnvironment() here is the LOGIC
                  // environment, a different object), and StartDac/StopDac
                  // (0x82680F50 / 0x82680FE8) are declaration-only on
                  // CgsSound::Playback::Environment. NOT reconstructed; reported rather
                  // than pointed at the wrong environment -- silencing the DAC through
                  // the wrong object would kill ALL audio.
            break;

        case 256:  // A GUI state whose +440 word is 15 asks for online VO 1.
            if (EventS32At(lpEvent, 440) == 15)
            {
                PostSoundMessage(mMessageQueue, 37, 0, 0, 5,
                                 CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT,
                                 static_cast<s32>(1));
            }
            break;

        case 298:  // A sequence + its 64-bit "new rival" payload.
            PostSoundMessage(mMessageQueue, 28, 0, 0, 2,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT,
                             CgsSound::Playback::Name(
                                 static_cast<uintptr_t>(
                                     static_cast<u32>(EventS32At(lpEvent, 0)))));
            PostSoundMessage(mMessageQueue, 25, 0, 0, 5,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT,
                             EventU64At(lpEvent, 8));
            break;

        case 302:  // Speech: the "car won" 64-bit id.
            PostSoundMessage(mMessageQueue, 35, 0, 0, 5,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT,
                             EventU64At(lpEvent, 0));
            break;

        case 303:  // Rank-up: rank 6 with the "elite" flag gets its own trigger VO.
            if (EventS32At(lpEvent, 4) && EventS32At(lpEvent, 0) == 6)
            {
                PostSoundMessage(mMessageQueue, 36, 0, 0, 5,
                                 CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT,
                                 CgsSound::Playback::Name(
                                     CgsSound::Playback::Name::MakeHash(
                                         "100_Percent_And_Elite")));
            }
            else
            {
                PostSoundMessage(mMessageQueue, 27, 0, 0, 5,
                                 CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT,
                                 EventS32At(lpEvent, 0));
            }
            break;

        case 458:  // GuiEventAudioTraxUpdate -- the EA Trax playlist masks. ⭐ This is
                   // the event that tells the music effect which songs it may play; with
                   // it dropped both masks stayed zero and SelectSong could only ever
                   // return -1.
        {
            CGS_ASSERT(lpEvent != 0, "lpTraxEvent");
            GuiAudioTraxUpdatePayload lPayload;
            std::memcpy(lPayload.maData, lpuPayload, sizeof(lPayload.maData));
            PostSoundMessage(mMessageQueue, 7, 0, 0, 2,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT, lPayload);
            break;
        }

        case 459:  // GuiEventAudioTraxLastPlayedIndexes -- the saved playlist cursor.
        {
            CGS_ASSERT(lpEvent != 0, "lpTraxEvent");
            GuiAudioTraxIndexesPayload lPayload;
            std::memcpy(lPayload.maData, lpuPayload, sizeof(lPayload.maData));
            PostSoundMessage(mMessageQueue, 8, 0, 0, 2,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT, lPayload);
            break;
        }

        case 460:  // GuiEventAudioTraxPreview.
        {
            CGS_ASSERT(lpEvent != 0, "lpPreviewEvent");
            GuiAudioTraxPreviewPayload lPayload;
            std::memcpy(lPayload.maData, lpuPayload, sizeof(lPayload.maData));
            PostSoundMessage(mMessageQueue, 9, 0, 0, 2,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT, lPayload);
            break;
        }

        case 461:  // "Next track".
            PostSoundMessage(mMessageQueue, 10, 0, 0, 2,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT, true);
            break;

        case 462:  // GuiEventAudioTraxPlayOrder.
            CGS_ASSERT(lpEvent != 0, "lpPlayOrderEvent");
            PostSoundMessage(mMessageQueue, 11, 0, 0, 2,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT,
                             EventS32At(lpEvent, 0));
            break;

        case 463:  // GuiEventAudioSettings -> the mixer CONTROL (effect id 0).
        {
            CGS_ASSERT(lpEvent != 0, "lpSettingsEvent");
            // The console (0x826EE508: `ld r11,0(r30); std r11,var_1E0`) copies the
            // payload's EIGHT bytes {music, sfx} into Message<GuiEventAudioSettings>
            // (AddEvent size 12 words == 16 + 8). MixerControl::Notify @0x826D2480 reads
            // BOTH (+16 music, +20 sfx); a 4-byte Message<s32> here left the consumer's
            // sfx word past the record -- the mixer scaled every SFX channel by garbage.
            GuiAudioSettingsPayload lPayload;
            std::memcpy(lPayload.maData, lpuPayload, sizeof(lPayload.maData));
            PostSoundMessage(mMessageQueue, 12, 0, 0, 0,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_CONTROL, lPayload);
            break;
        }

        case 464:  // GuiEventAudioEventIntros -> speech.
        {
            CGS_ASSERT(lpEvent != 0, "lpIntroEvent");
            GuiAudioEventIntrosPayload lPayload;
            std::memcpy(lPayload.maData, lpuPayload, sizeof(lPayload.maData));
            PostSoundMessage(mMessageQueue, 31, 0, 0, 5,
                             CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT, lPayload);
            break;
        }

        case 465:  // GuiEventAudioResults: the console writes the event's first two
                   // words straight into the sound root INPUT buffer at +79664 (the
                   // GUI-audio results slot the effects poll), not into a message. That
                   // slot has no named accessor in this tree; NOT reconstructed, and
                   // reported rather than guessed at.
            break;

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
        // [DIAG] NOT IN THE X360 BINARY -- BRN_ENGINE_DIAG car-audio request witness.
        if (std::getenv("BRN_ENGINE_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "[car-audio] sound recv type=" << static_cast<s32>(lrEvent.meMessageType)
                                       << " car=" << static_cast<s32>(lrEvent.miActiveRaceCarIndex)
                                       << " asset=" << lrEvent.mAssetID
                                       << " isPlayer=" << (lrEvent.mbIsPlayer ? 1 : 0) << "\n";
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

// X360 0x826EC400. The game -> sound dispatch: every game action the game module posts
// is turned into the sound message(s) the console builds for it. THIS FUNCTION HANDLED
// EXACTLY ONE OF THE CONSOLE'S 28 CASES (297) before 2026-09-15, which is why most
// in-game one-shot audio never sounded -- the speech, HUD, FX, music and mixer cues were
// all dropped on the floor here, silently.
//
// Message construction convention, read off the console's stack images: each case fills
// a CgsSound::Io::Message<T> whose header is { +0x04 effect type, +0x08 event id,
// +0x0A state-manager id, +0x0C instance id, +0x0E effect id } and whose payload starts
// at +0x10, then calls VariableEventQueue<8192,16>::AddEvent(mMessageQueue, msg, id).
// Effect type 1 == E_EFFECT_TYPE_OBJECT, 2 == E_EFFECT_TYPE_CONTROL.
//
// Cases that route into an effect this wave does not own (speech 34/36/37/38, HUD 21/24,
// FX 4, collision 2, ...) are routed EXACTLY as the console routes them; the effect body
// on the far end belongs to its own owner.
void SoundLogicModule::ProcessGameActionQueue(
    EActiveRaceCarIndex /*aePlayerCarIndex*/,
    const Io::RootInputBuffer::GameActionQueue& arEvents)
{
    typedef CgsSound::Io::MessageHeader MH;

    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    s32 liType = arEvents.GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        switch (liType)
        {
        case 0:   // ResetPlayerCarAction -- reset the junkyard car, twice (two effects).
        {
            const s32 leCarSelectType = EventS32At(lpEvent, 60);
            PostSoundMessage(mMessageQueue, 41, 0, 0, 6, MH::E_EFFECT_TYPE_OBJECT,
                             leCarSelectType);
            PostSoundMessage(mMessageQueue, 41, 1, 0, 9, MH::E_EFFECT_TYPE_OBJECT,
                             leCarSelectType);
            break;
        }

        case 16:  // The action's flag-0x80 arm: publish its +0x1C field into the module's
                  // dispatch state block (see maDispatchState's banner).
            if ((EventU8At(lpEvent, 0) & 0x80u) == 0x80u)
            {
                const s32 liField = EventS32At(lpEvent, 28);   // X360 *(v9 + 7), dwords
                if (liField != -1)
                    mDispatchState.mu8FlagAt1 = 1;
                mDispatchState.mi32At4 = liField;
            }
            break;

        case 29:  // GameModeStarted.
            if (EventU8At(lpEvent, 149) < 10)
            {
                // Speech fade-stop over 2 s before the mode's own VO starts.
                PostSoundMessage(mMessageQueue, 38, 0, 0, 5, MH::E_EFFECT_TYPE_OBJECT,
                                 2.0f);
            }
            // X360 falls straight through into the generic broadcast below.
            PostSoundMessage(mMessageQueue, 3, 0, 0, 1, MH::E_EFFECT_TYPE_CONTROL,
                             liType);
            break;

        case 33:
        case 35:
        case 39:
            // The generic "this game action happened" broadcast (message 3, addressed to
            // control 1). The payload IS the game action id.
            PostSoundMessage(mMessageQueue, 3, 0, 0, 1, MH::E_EFFECT_TYPE_CONTROL,
                             liType);
            break;

        case 37:  // ShowModeResultsAction -- the results screen.
        {
            CGS_ASSERT(lpEvent != 0, "lpModeResultsAction");
            ShowModeResultsPayload lResults;
            std::memcpy(lResults.maData, lpEvent, sizeof(lResults.maData));
            PostSoundMessage(mMessageQueue, 23, 0, 0, 2, MH::E_EFFECT_TYPE_OBJECT,
                             lResults);

            const s32 leGameMode = EventS32At(lpEvent, 0);
            if (leGameMode < 10)
            {
                const bool lbSkip = (leGameMode == 5) ? (EventU8At(lpEvent, 225) == 0)
                                                      : (EventU8At(lpEvent, 224) != 0);
                if (!lbSkip)
                {
                    BrnSound::GameModeLostResults lLost;
                    lLost.meGameMode = leGameMode;
                    lLost.miNumLossesForGameMode = EventS32At(lpEvent, 212);
                    PostSoundMessage(mMessageQueue, 32, 0, 0, 5,
                                     MH::E_EFFECT_TYPE_OBJECT, lLost);
                }
            }
            // X360: mode 12 (the 8-racer online mode) walks the AI-car input block
            // comparing two accumulated scores and only reports a loss when the player's
            // is the lower one. That scan reads the root input's AI-car interface at
            // +2612 / +212 (296-byte stride, 8 entries) through two getters this tree
            // does not expose; NOT reconstructed, and reported rather than approximated.
            break;
        }

        case 40:  // QuitEvent -> FxEffect.
            PostSoundMessage(mMessageQueue, 4, 0, 0, 3, MH::E_EFFECT_TYPE_OBJECT,
                             static_cast<s32>(7));
            break;

        case 56:  // Stunt jump: only for the eleven authored jump keys.
        {
            const u32 luKey = static_cast<u32>(EventS32At(lpEvent, 4));
            for (u32 luEntry = 0; luEntry < 11; ++luEntry)
            {
                if (KA_STUNT_JUMP_KEYS[luEntry] == luKey)
                {
                    PostSoundMessage(mMessageQueue, 4, 0, 0, 3,
                                     MH::E_EFFECT_TYPE_OBJECT, static_cast<s32>(4));
                }
            }
            break;
        }

        case 58:  // Stunt smash / stunt.
        {
            const s32 liKind = EventS32At(lpEvent, 8);
            if (liKind == 1)
                PostSoundMessage(mMessageQueue, 4, 0, 0, 3, MH::E_EFFECT_TYPE_OBJECT,
                                 static_cast<s32>(2));
            else if (liKind == 2)
                PostSoundMessage(mMessageQueue, 4, 0, 0, 3, MH::E_EFFECT_TYPE_OBJECT,
                                 static_cast<s32>(3));
            break;
        }

        case 62:  // Unlock sting: "shutdown" for kind 3, "liveryunlock" otherwise.
        {
            const s32 liKind = EventS32At(lpEvent, 8);
            const char* lpcName = (liKind != 3) ? "liveryunlock" : "shutdown";
            PostSoundMessage(mMessageQueue, 28, 0, 0, 2, MH::E_EFFECT_TYPE_OBJECT,
                             CgsSound::Playback::Name(
                                 CgsSound::Playback::Name::MakeHash(lpcName)));
            break;
        }

        case 73:  // Mixer snapshot 6 on.
            if (EventU8At(lpEvent, 0))
                GetEnvironment().GetDynamicMixer().SetSnapshot(6, true);
            break;

        case 77:  // Mixer snapshot 6 off.
            GetEnvironment().GetDynamicMixer().SetSnapshot(6, false);
            break;

        case 86:
        case 88:
            mDispatchState.mu64Flags |= 1u;                       // @0x826ECEC8 `ori 1`
            break;

        case 87:
        case 89:
            mDispatchState.mu64Flags &= ~static_cast<u64>(1u);    // @0x826ECEE0 `clrrdi 1`
            break;

        case 97:  // PlayerCarRepaired -> HUD control.
            PostSoundMessage(mMessageQueue, 21, 1, 0, 3, MH::E_EFFECT_TYPE_CONTROL,
                             true);
            break;

        case 99:  // Entering / leaving the junkyard.
        {
            const u8 lu8InJunkyard = EventU8At(lpEvent, 0);
            s32 leAmbience = 0;
            if (lu8InJunkyard)
            {
                leAmbience = 2;
            }
            else
            {
                leAmbience = 0;
                // Leaving also clears the menu stream (K_NULL_NAME == 0).
                BrnSound::MusicOnMenuStreamData lClear;
                lClear.muStreamNameHash = 0u;
                lClear.mbFromVideo = 0;
                lClear.mbPlayOverCustomSoundtrack = 0;
                lClear.maReserved[0] = 0;
                lClear.maReserved[1] = 0;
                PostSoundMessage(mMessageQueue, 13, 0, 0, 2, MH::E_EFFECT_TYPE_OBJECT,
                                 lClear);
            }
            PostSoundMessage(mMessageQueue, 40, 0, 0, 2, MH::E_EFFECT_TYPE_OBJECT,
                             leAmbience);
            PostSoundMessage(mMessageQueue, 40, 0, 0, 6, MH::E_EFFECT_TYPE_OBJECT,
                             leAmbience);
            break;
        }

        case 106:  // Junkyard, standard ambience, unconditionally.
            PostSoundMessage(mMessageQueue, 40, 0, 0, 2, MH::E_EFFECT_TYPE_OBJECT,
                             static_cast<s32>(2));
            PostSoundMessage(mMessageQueue, 40, 0, 0, 6, MH::E_EFFECT_TYPE_OBJECT,
                             static_cast<s32>(2));
            break;

        case 146:  // Showtime intro.
            PostSoundMessage(mMessageQueue, 24, 0, 0, 2, MH::E_EFFECT_TYPE_OBJECT, true);
            break;

        case 148:  // GameTrainingAction -- the first-time tip VO for this training type.
            CGS_ASSERT(lpEvent != 0, "lpGameTrainingAction");
            // The console loads the training type as a 32-bit word (lwz); a byte read
            // happened to agree on little-endian for every type < 256 -- read the word.
            PostSoundMessage(mMessageQueue, 34, 0, 0, 5, MH::E_EFFECT_TYPE_OBJECT,
                             EventS32At(lpEvent, 0));
            break;

        case 153:  // Online voice-over triggers.
        {
            const s32 liKind = EventS32At(lpEvent, 8);
            if (liKind == 1)
            {
                const s32 leVo = (EventU8At(lpEvent, 28) == 0) ? 0 : 3;
                PostSoundMessage(mMessageQueue, 37, 0, 0, 5, MH::E_EFFECT_TYPE_OBJECT,
                                 leVo);
            }
            else if (liKind == 5)
            {
                const s32 liSub = EventS32At(lpEvent, 12);
                if (liSub != 2 || EventU8At(lpEvent, 29))
                {
                    if (liSub == 1)
                        PostSoundMessage(mMessageQueue, 37, 0, 0, 5,
                                         MH::E_EFFECT_TYPE_OBJECT, static_cast<s32>(5));
                }
                else
                {
                    PostSoundMessage(mMessageQueue, 37, 0, 0, 5,
                                     MH::E_EFFECT_TYPE_OBJECT, static_cast<s32>(4));
                }
            }
            break;
        }

        case 202:  // "Find all events" trigger VO.
            PostSoundMessage(mMessageQueue, 36, 0, 0, 5, MH::E_EFFECT_TYPE_OBJECT,
                             CgsSound::Playback::Name(
                                 CgsSound::Playback::Name::MakeHash("Find_All_Events")));
            break;

        case 204:  // TrophyUnlockData.
            PostSoundMessage(mMessageQueue, 30, 0, 0, 5, MH::E_EFFECT_TYPE_OBJECT,
                             EventS32At(lpEvent, 8));
            break;

        case 205:  // RoadRagePlayerDamageAction -> the player-damage control.
        {
            RoadRageDamagePayload lDamage;
            std::memcpy(lDamage.maData, lpEvent, sizeof(lDamage.maData));
            PostSoundMessage(mMessageQueue, 20, 1, 0, 3, MH::E_EFFECT_TYPE_CONTROL,
                             lDamage);
            break;
        }

        case 218:  // SoundTriggerAction -- the per-frame trigger table
                   // GetSoundTriggerAction searches. (X360 also mirrors it into
                   // BrnReplays::SoundSerialiser's 512-event queue while the replay
                   // recorder state is 1/2/3; that serialiser is not in this tree, so
                   // only the live half runs.)
        {
            BrnGameState::GameStateModuleIO::SoundTriggerAction lAction;
            std::memset(&lAction, 0, sizeof(lAction));
            const size_t luCopy = (liSize > 0 &&
                                   static_cast<size_t>(liSize) < sizeof(lAction))
                                      ? static_cast<size_t>(liSize) : sizeof(lAction);
            std::memcpy(&lAction, lpEvent, luCopy);
            maTriggerActions.Append(lAction);
            break;
        }

        case 287:  // Mixer snapshot 15 from the action's own flag.
            GetEnvironment().GetDynamicMixer().SetSnapshot(
                15, EventU8At(lpEvent, 0) != 0);
            break;

        case 297:  // Bind the collision resolver to the prop-physics table.
        {
            CgsSound::Io::Message<BrnSound::ESoundMessages> lMessage;
            lMessage.Construct(
                BrnSound::E_SOUNDMESSAGE_COLLISION_BIND_TO_PROPS,
                5,
                MH::KU16_NO_DESTINATION,
                MH::KU16_NO_DESTINATION,
                MH::E_EFFECT_TYPE_NONE);
            lMessage.mData = BrnSound::E_SOUNDMESSAGE_COLLISION_BIND_TO_PROPS;
            QueueSoundMessage(mMessageQueue, lMessage);
            break;
        }

        default:
            break;
        }

        liType = arEvents.GetNextEvent(lpEvent, &lpEvent, &liSize);
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

    // X360 @0x826B01B4..0x826B01C4: `clrlwi r9, updateSet, 31` -> +0x13578 (the sim-paused
    // bit, the other half of MusicEffect::UpdateParams' stream-pause gate) and `stb 0` ->
    // +0x13579 (game action 16's one-shot flag, cleared every tick).
    mDispatchState.mu8SimPausedBit = static_cast<u8>(aeUpdateSet & 1u);
    mDispatchState.mu8FlagAt1      = 0;

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
    const Io::RootInputBuffer* lpReadInput = lpInput;
    ProcessGameActionQueue(lePlayerCarIndex, lpReadInput->GetGameActionQueue());
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
