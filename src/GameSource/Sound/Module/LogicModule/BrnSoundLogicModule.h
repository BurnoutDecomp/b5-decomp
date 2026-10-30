#ifndef BRN_SOUND_MODULE_BRN_SOUND_LOGIC_MODULE_H
#define BRN_SOUND_MODULE_BRN_SOUND_LOGIC_MODULE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                              // EntityId
#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>
#include "GameSource/GameState/BrnGameActions.h"         // BrnGameState::GameStateModuleIO::SoundTriggerAction (element home)
// IResourceRequester (base) + ResourceRegistrar (member) from their CANONICAL home
// (BrnResourceRegistrar.h), NOT BrnEffectObject.h: the latter ALSO defines a
// CgsSound::Logic::ClassTypeInfo template that would clash with the canonical one in
// CgsStateManager.h (pulled in via CgsEnvironment.h below) once both land in this TU.
// BrnSoundLogicModule only needs IResourceRequester/ResourceRegistrar, so it takes the
// canonical home directly and avoids the ODR-split template entirely.
#include "GameSource/Sound/BrnResourceRegistrar.h"               // BrnSound::Logic::IResourceRequester (base) + ResourceRegistrar (member)
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"   // CgsSound::Logic::Environment + canonical CgsSound::Logic::StateManager (the 9-slot map + the CreateStateMan factory)
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h" // CgsSound::Logic::Module -- the REAL engine base (phase B5)
#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"            // the three stage-2 logic voices
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"           // Io::PreUpdateOutput (the by-value pre-update block; phase C1)
#include "GameSource/AttribSys/Generated/classes/burnoutglobaldata.h"
#include "GameSource/Sound/Collision/BrnCollisionFrameInformation.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Replays/Serialisers/BrnReplaySoundSerialiser.h"
#include "SharedClasses/BrnSharedConstants.h"                    // BrnUpdateSet

// Native SoundLogicModule layout is pinned by ARTIST827E3DA8 construction,
// Construct826C9330, field readers and CheckClassSizes823C01A4 (sizeof13680).
// DecFIGS supplies the shared declaration order; its PS3-only shape does not
// override the native replay serialiser or the convoy fields in FrameInformation.

namespace BrnSound
{
namespace Module
{

// Forward declarations of the two buffer pointer members SoundLogicModule holds
// (DWARF BrnSoundLogicModule.h:365-366). Their full layouts live with the IO TU;
// only the pointer is stored here.
namespace Io
{
    // (phase C3b: LogicInputBuffer is no longer forward-declarable -- it is the
    // DWARF typedef of RootInputBuffer, provided by the BrnRootSoundModuleIo.h
    // include above.)
    class LogicOutputBuffer;
    struct LogicPreUpdateOutputBuffer;   // the PreUpdate scratch (BrnSoundLogicModuleIo.h)
}

// (2026-08-25, faithful-audio-engine phase B5): the class now derives the REAL
// CgsSound::Logic::Module engine base as its PRIMARY base (console +0x00..0x4C8F
// -- the base's mPlaybackModule at +0x238 is exactly the playback module the
// RootSoundModule reaches at root+0x4B8 == +0x280 + 0x238), with
// IResourceRequester as the SECOND base at console +0x4C90 (the wave-1
// "GetResourceRegistrar +0x4C90 sub-object walk" -- its @0x826838C0 `addi r3,
// 0x588` is relative to THAT sub-object: 0x4C90 + 0x588 == mResourceRegistrar
// @+0x5218). The former by-value lEnvironment member is RETIRED: the LOGIC
// Environment lives in the engine base (+0x2950), reached via GetEnvironment().
struct SoundLogicModule : public CgsSound::Logic::Module,
                          public BrnSound::Logic::IResourceRequester
{
    // BrnSoundLogicModule.h:58 (DWARF).
    static const s32 KI_MAX_SOUND_TRIGGER_ACTIONS = 16;

    // The fixed bank of sound-logic state managers. Proven by CreateStateManagers
    // (X360 0x826AFEF8) iterating i=0..8 over mapStateManagers (this+79064) and by
    // PrepareStateManagersOnBoot (0x826837F8) iterating the same 9 slots.
    static const s32 KI_NUM_STATE_MANAGERS = 9;

    // Prepare82703C18 persists its stage at native byte134FC.
    // Stage order mirrors the X360 switch: AddMonitor -> base Module::Prepare ->
    // construct+connect the 3 Voices + LoadAsset(BurnoutGlobalData) -> ResourceBridging ->
    // CreateStateManagers -> PrepareStateManagersOnBoot -> done.
    enum EPrepareStage
    {
        E_PREPSTAGE_PERFMON       = 0,  // case 0: CgsDev::PerfMonCpu::AddMonitor("Resource Registrar")
        E_PREPSTAGE_BASE          = 1,  // case 1: CgsSound::Logic::Module::Prepare (base)
        E_PREPSTAGE_VOICES        = 2,  // case 2: Voice::Construct/Connect x3 + IResourceRequester::LoadAsset
        E_PREPSTAGE_BRIDGE        = 3,  // case 3: ResourceBridging (under IOBuffer lock)
        E_PREPSTAGE_STATEMANAGERS = 4,  // case 4: CreateStateManagers
        E_PREPSTAGE_BOOTPREPARE   = 5,  // case 5: PrepareStateManagersOnBoot(4)
        E_PREPSTAGE_DONE          = 6,  // case 6: prepared
    };

    enum EReleaseStage
    {
        E_BRN_RELEASE_START = 0,
        E_BRN_RELEASE_SELF = 1,
        E_BRN_RELEASE_MASTER_VOICE = 2,
        E_BRN_RELEASE_DONE = 3
    };

    SoundLogicModule();

    // Bring-up (X360 ctor 0x827E3DA8 + the registrar bring-up 0x826B0470): the engine
    // base Construct (chains ModuleSingleBuffered + the message queue + the embedded
    // playback module's seeds), then clear the trigger table + Construct the embedded
    // ResourceRegistrar so the broker's queues/pools are live. The 3 Voices are still
    // grown on top. (Overrides the engine base's virtual Construct -- the root
    // dispatches vtable slot 0 on this+0x280.)
    void Construct() ;

    // Prepare (X360 0x82703C18, via vtable+0x58). The REAL resumable stage machine
    // (phase B5): capture the logic RW allocator; AttachBuffers; then per the console
    // switch -- perfmon (0), the ENGINE base Module::Prepare with the 0x820AA480
    // rodata ModuleParams {16,16,16} (1), the 3 Voices + LoadAsset (2 -- still a
    // FLAG'd grow-in), ResourceBridging under the output-buffer write lock (3),
    // CreateStateManagers (4), PrepareStateManagersOnBoot(4) (5). The console
    // advances ONE chunk per call in the early stages (returns false after 0+1 and
    // after the voice constructs), detaching the buffers at every exit.
    virtual bool Prepare(rw::IResourceAllocator* apAllocator,
                 CgsModule::IOBuffer* apInputBuffer,
                 CgsModule::IOBuffer* apOutputBuffer);

    bool PrepareOnEnteringGameplay(rw::IResourceAllocator* apAllocator,
                                   CgsModule::IOBuffer* apInputBuffer,
                                   CgsModule::IOBuffer* apOutputBuffer);

    // BrnSoundLogicModule.cpp:522 (DWARF). The Burnout update carries the
    // frame's update-set in addition to the generic logic-engine arguments.
    virtual void Update(f32 af32GameDt, f32 af32SimDt,
                CgsModule::IOBuffer* apInputBuffer,
                CgsModule::IOBuffer* apOutputBuffer,
                BrnUpdateSet aeUpdateSet);

    // ARTIST 0x826978A0 / 0x826978B8. The derived attachment mirrors the
    // generic engine buffer pointers into the Burnout-specific typed pair.
    void AttachBuffers(CgsModule::IOBuffer* apInputBuffer,
                       CgsModule::IOBuffer* apOutputBuffer) ;
    void DetachBuffers() ;

    // The engine base sizes the LOGIC Environment's state-manager map from this
    // (Prepare's CreateEnvironment stage: {allocator, GetNumberOfStates()}); the 9
    // AddStateManager registrations in CreateStateManagers demand exactly the 9.
    virtual s32 GetNumberOfStates()  { return KI_NUM_STATE_MANAGERS; }

    // @ 0x826838C8. The Burnout override of the engine's id generator: hand out the
    // next id from THIS module's own cursor (X360 this+0x5214), skipping the 19
    // RESERVED idents. Bodied in BrnSoundLogicModule.cpp.
    virtual u32 GetUniqueId() ;

    // @ 0x826E1F10 (DWARF :152; phase C1). Publish the module's accumulated
    // pre-update output block into the caller's scratch buffer (write-locked
    // SetPreUpdateOutput copy; assert cpp:495).
    void PreUpdate(Io::LogicPreUpdateOutputBuffer* apLogicPreUpdateOutput);

    // DWARF :194 -- the pre-update output block, by reference (the state
    // managers/effects accumulate into it each frame).
    BrnSound::Module::Io::PreUpdateOutput& GetPreUpdateOutput() { return mPreUpdateOutput; }

    // X360 0x82702E80. Per-frame resource bridge: drive the embedded ResourceRegistrar's Update
    // (drain its request queues, resolve handles, GC unreferenced files), then bridge its two
    // request-interface queues into the logic output buffer. Called from Prepare stage 3 + the
    // per-frame update.
    void ResourceBridging();

    // X360 0x826AFEF8 (Prepare stage 4). Create the 9 sound-logic state managers: loop
    // i=0..8 -> mapStateManagers[i] = StateManager::CreateStateMan(i, this); when the
    // factory returns a manager (i.e. a leaf is registered for that id), register it into
    // the embedded Environment (lEnvironment.AddStateManager). Null slots (no leaf
    // registered) are skipped, so this is a safe no-op while the manager TUs are out of
    // the build (the registry is empty -> every CreateStateMan returns null).
    void CreateStateManagers();

    // X360 0x826837F8 (Prepare stage 5, boot mask = 4). Drive each created manager's
    // Prepare() (vtable +0x0C), skipping the slots whose bit is set in luSkipMask; if any
    // manager's Prepare() returns false, return false (the boot stage retries). Then the
    // mapStateManagers[0] child-prepare special-case (its GetChildStateManager(0), vtable
    // +0x14 -> if non-null, the child's Prepare(), vtable +0x0C). Returns true on success.
    bool PrepareStateManagersOnBoot(s32 luSkipMask);

    // X360 0x826EC108. The TWIN of PrepareStateManagersOnBoot, with the OPPOSITE mask
    // sense: it prepares ONLY the slots whose bit is SET. Boot calls the skip-variant with
    // mask 4, so slot 2 -- AIVehicleStateManager -- is the one manager boot never prepares;
    // this is the call that prepares it, and the console passes it the same literal 4.
    // Its console caller chain is SoundLogicModule::PrepareOnEnteringGameplay @0x82703F18
    // <- RootSoundModule::PrepareOnEnteringGameplay @0x82704188 <-
    // LoadingScriptedState::LoadSoundModuleAgain @0x823E7700; see the body for why the
    // poll is driven from Update() here instead.
    bool PrepareStateManagersOnEnteringGameplay(s32 luPrepareMask);

    // BrnSoundLogicModule.cpp:964 (DWARF). Linear-search the per-frame trigger-action
    // table for the entry that matches (leEntityId, leType), returning it (or null).
    // X360 0x826AFF88: walks maTriggerActions[0..count), comparing element.mEntityId
    // (+0x10) against leEntityId and element.meResultType (+0x14) against leType; the
    // first full match wins and the loop stops (the `do { } while(!result)` form).
    const BrnGameState::GameStateModuleIO::SoundTriggerAction*
        GetSoundTriggerAction(EntityId leEntityId,
                              BrnGameState::GameStateModuleIO::SoundTriggerAction::eType leType);

    // BrnSoundLogicModule.cpp:885 (DWARF). IResourceRequester override: return the
    // embedded registrar by reference. X360 0x826838C0: &mResourceRegistrar.
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar();

    // IResourceRequester completion callback. BurnoutGlobalData is loaded during
    // Prepare stage 2; this callback rebinds the generated instance to the now-live
    // AttribSys collection so the global effects can consume its RefSpecs.
    virtual void ResourcesAreReady() ;

    Attrib::Gen::burnoutglobaldata& GetGlobalData() { return mBurnoutGlobalData; }
    const Attrib::Gen::burnoutglobaldata& GetGlobalData() const { return mBurnoutGlobalData; }
    BrnSound::Logic::FrameInformation& GetFrameInformation() { return mFrameInformation; }
    const BrnSound::Logic::FrameInformation& GetFrameInformation() const { return mFrameInformation; }
    // Inlined in ReverbEffect::ProcessUpdate (ARTIST module+0x51FC).
    CgsSound::Logic::Voice& GetGlobalReverbVoice() { return mGlobalReverbVoice; }
    CgsNumeric::Random& GetRandomGenerator() { return mRandomGenerator; }
    const Attrib::RefSpec& GetSampleTags(u32 auTag) const
    {
        return mBurnoutGlobalData.SampleTags(auTag);
    }

    // The freed-ids list, by reference (the root Update's append target).
    CgsSound::Playback::Module::Io::OutputBuffer::FreedBuffersArray& GetFreedStreamBufferIds()
    {
        return mFreedStreamBufferIds;
    }

    // BrnSoundLogicModule.h:423 (DWARF). Hand out the sound logic input buffer the
    // module reads each frame, asserting it has been attached. X360 0x82682518:
    // loads mpBrnLogicInputBuffer (this+0x4C94), fires the "mpBrnLogicInputBuffer"
    // assert when null, then returns it. Non-const overload (the const overload at
    // h:431 is its own function and is not part of this slice).
    Io::LogicInputBuffer* GetBrnInputStructure();

    void ProcessGuiEvents(const CgsModule::VariableEventQueue<18432, 16>* apGuiEvents);

private:
    // ARTIST 0x826C9860 / 0x826B0040. Populate the two listener matrices and
    // the collision/world per-frame player snapshot before state-manager update.
    void UpdateMicrophones(const Io::LogicInputBuffer* apLogicInputBuffer);
    void UpdateFrameInformation(f32 af32SimDt,
                                const Io::LogicInputBuffer* apLogicInputBuffer,
                                BrnUpdateSet aeUpdateSet,
                                EActiveRaceCarIndex aePlayerCarIndex);

    // ARTIST 0x826EC250. Broadcast each playback-freed stream voice id to the
    // three StreamingState instances as sound message 16.
    void ProcessStreamFreedQueue(
        const CgsSound::Playback::Module::Io::OutputBuffer::FreedBuffersArray& arFreedIds);
    void ProcessCarDataLoadingQueue(const Io::AudioCarLoadedDataQueue& arEvents);
    void ProcessGameActionQueue(
        EActiveRaceCarIndex aePlayerCarIndex,
        const Io::RootInputBuffer::GameActionQueue& arEvents);

    // Turn each game event 31 (VehicleImpactEvent) into sound message 19 for every
    // instance of state manager 2, effect object 4. No other game event is handled.
    void ProcessGameEventQueue(EActiveRaceCarIndex aePlayerCarIndex,
                               const Io::RootInputBuffer::GameEventQueue* apEvents);

    // React to director-camera flag edges: the junkyard flash and new-car-intro stings,
    // the online-intro rival sweep and the online race start trigger.
    void ProcessCameraFlags(const Io::RootInputBuffer::DirectorCamera& arCamera);

private:
    // Native member order: ARTIST827E3DA8 constructor and826C9330 Construct.
    Io::LogicInputBuffer* mpBrnLogicInputBuffer;   // +4C94
    Io::LogicOutputBuffer* mpBrnLogicOutputBuffer; // +4C98
    Array<BrnGameState::GameStateModuleIO::SoundTriggerAction,16> maTriggerActions; // +4CA0
    Io::PreUpdateOutput mPreUpdateOutput;         // +4EB0
    CgsSound::Playback::Module::Io::OutputBuffer::FreedBuffersArray mFreedStreamBufferIds; // +51E0
    CgsSound::Logic::Voice mMasterVoice;           // +51F0
    CgsSound::Logic::Voice mGlobalReverbVoice;     // +51FC
    CgsSound::Logic::Voice mSubmixVoice;           // +5208
    u32 muBrnUniqueId;                            // +5214
    BrnSound::Logic::ResourceRegistrar mResourceRegistrar; // +5218
    CgsSound::Logic::StateManager* mapStateManagers[KI_NUM_STATE_MANAGERS]; // +134D8
    EPrepareStage meBrnPrepareStage;              // +134FC
    EReleaseStage meBrnReleaseStage;              // +13500
    rw::IResourceAllocator* mpBrnAllocator;        // +13504
    Attrib::Gen::burnoutglobaldata mBurnoutGlobalData; // +13508
    BrnSound::Logic::FrameInformation mFrameInformation; // +13520
    CgsNumeric::Random mRandomGenerator;           // +13590
    bool mbSeenAnyRivals;                         // +135C0
    s32 miCommandStreamUsage;                     // +135C4
    s32 miResourceRegistrarMonitor;               // +135C8
    f32 mfHoldTimer;                             // +135CC
    bool mbHoldingVolumes;                       // +135D0

    // The actual replay serialiser is cache-line aligned. Construct826C9350
    // calls SoundSerialiser::Construct8264C9F8 at13600; CheckClassSizes823C01A4
    // independently gives SoundLogicModule sizeof13680.
    CGS_ALIGN(128) BrnReplays::SoundSerialiser mSoundSerialiser; // +13600

public:
    CgsSound::Logic::Voice& GetMasterVoice() { return mMasterVoice; }
    const CgsSound::Logic::Voice& GetMasterVoice() const { return mMasterVoice; }

    bool AreMusicStreamsPaused() const
    {
        return !mFrameInformation.maPaused.IsZero() && !mFrameInformation.mbInReplay;
    }

};

} // namespace Module
} // namespace BrnSound

#endif // BRN_SOUND_MODULE_BRN_SOUND_LOGIC_MODULE_H
