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
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"           // Io::PreUpdateOutput (the by-value pre-update block; phase C1)

// =============================================================================
// BrnSound::Module::SoundLogicModule
//   GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. SoundLogicModule is the Burnout
// sound-logic module facade. It inherits BrnSound::Logic::IResourceRequester
// (DWARF: `struct SoundLogicModule : public BrnSound::Logic::IResourceRequester`)
// and so carries a vtable; GetResourceRegistrar() is the IResourceRequester
// override that hands out the embedded registrar.
//
// MINIMAL SLICE: only the two members this group's functions touch are modelled
// as real typed members, in DWARF source order:
//   * maTriggerActions  (Array<SoundTriggerAction,16>) -- searched by
//     GetSoundTriggerAction (X360 0x826AFF88, reads the array base at this+0x4CA0;
//     element stride 32, count word at array+0x200, matching the committed
//     Array<T,N> shape).
//   * mResourceRegistrar (BrnSound::Logic::ResourceRegistrar) -- returned by
//     GetResourceRegistrar (X360 0x826838C0: `addi r3, r3, 0x588; blr`, i.e.
//     &mResourceRegistrar at this+0x588).
// The earlier DWARF members (mpBrnLogicInputBuffer, mpBrnLogicOutputBuffer) are
// modelled as their real pointer types so maTriggerActions follows them in
// sequence; the members between maTriggerActions and mResourceRegistrar, and the
// SoundLogicModule tail, are not touched by this group and are intentionally
// omitted from this slice (they land with the full SoundLogicModule TU).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 ASM reaches maTriggerActions
// at this+0x4CA0 and mResourceRegistrar at this+0x588 assuming 4-byte pointers +
// a 4-byte vptr. On a 64-bit host those widths differ, so members are pinned BY
// NAME and SEQUENCE only; absolute offsets are NOT static_asserted across the
// vtable / pointer members.
// FLAG: minimal slice -- the omitted members (DWARF BrnSoundLogicModule.h:374-401)
// are reconstructed by the full SoundLogicModule TU; do not duplicate them here.
// =============================================================================

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

    // The Prepare resumable-stage machine (X360 0x82703C18, stage word @ this+19775).
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

    SoundLogicModule()
        : CgsSound::Logic::Module()
        , mpBrnLogicInputBuffer(0)
        , mpBrnLogicOutputBuffer(0)
        , mpBrnAllocator(0)
        , meBrnPrepareStage(E_PREPSTAGE_PERFMON)
        , mbConstructed(false)
        , mbPrepared(false)
    {
    }
    virtual ~SoundLogicModule() {}

    // Bring-up (X360 ctor 0x827E3DA8 + the registrar bring-up 0x826B0470): the engine
    // base Construct (chains ModuleSingleBuffered + the message queue + the embedded
    // playback module's seeds), then clear the trigger table + Construct the embedded
    // ResourceRegistrar so the broker's queues/pools are live. The 3 Voices are still
    // grown on top. (Overrides the engine base's virtual Construct -- the root
    // dispatches vtable slot 0 on this+0x280.)
    void Construct() override;

    // Prepare (X360 0x82703C18, via vtable+0x58). The REAL resumable stage machine
    // (phase B5): capture the logic RW allocator; AttachBuffers; then per the console
    // switch -- perfmon (0), the ENGINE base Module::Prepare with the 0x820AA480
    // rodata ModuleParams {16,16,16} (1), the 3 Voices + LoadAsset (2 -- still a
    // FLAG'd grow-in), ResourceBridging under the output-buffer write lock (3),
    // CreateStateManagers (4), PrepareStateManagersOnBoot(4) (5). The console
    // advances ONE chunk per call in the early stages (returns false after 0+1 and
    // after the voice constructs), detaching the buffers at every exit.
    bool Prepare(rw::IResourceAllocator* apAllocator,
                 CgsModule::IOBuffer* apInputBuffer,
                 CgsModule::IOBuffer* apOutputBuffer);

    // The engine base sizes the LOGIC Environment's state-manager map from this
    // (Prepare's CreateEnvironment stage: {allocator, GetNumberOfStates()}); the 9
    // AddStateManager registrations in CreateStateManagers demand exactly the 9.
    virtual s32 GetNumberOfStates() override { return KI_NUM_STATE_MANAGERS; }

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

    // IResourceRequester override (makes the class concrete). FLAG: minimal stub -- the
    // X360 dumped no standalone SoundLogicModule::ResourcesAreReady; the real body lives in
    // the un-modeled CgsSound::Logic::Module base (this slice inherits only IResourceRequester).
    // The broker does not drive it on the minimal path; grown with the Voice/LoadAsset path.
    virtual void ResourcesAreReady() {}

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

private:
    // Members in DWARF source order (BrnSoundLogicModule.h:365-383). Only the two
    // touched members are real typed members; the rest of the class tail is omitted
    // from this slice (see header note).
    Io::LogicInputBuffer*  mpBrnLogicInputBuffer;   // BrnSoundLogicModule.h:365 (X360 this+0x4C94)
    Io::LogicOutputBuffer* mpBrnLogicOutputBuffer;  // BrnSoundLogicModule.h:366 (X360 this+0x4C98)

    // The logic RW allocator Prepare captures first (X360 word a1[19777] = a2 -- the
    // &gLogicTestBedAlloc the root's LOGIC stage passes; DWARF member name pending, held
    // by usage). Distinct from the ENGINE base's mpAllocator (which the base Prepare
    // adopts from the same argument).
    rw::IResourceAllocator* mpBrnAllocator;         // X360 word 19777 (byte +0x4D04)

    Array<BrnGameState::GameStateModuleIO::SoundTriggerAction, 16> maTriggerActions; // h:372 (X360 this+0x4CA0)

    BrnSound::Logic::ResourceRegistrar mResourceRegistrar; // h:383 (+0x588 from the IResourceRequester sub-object == X360 this+0x5218)

    // (phase B5: the by-value lEnvironment member is RETIRED -- the LOGIC Environment is
    // the ENGINE base's mEnvironment @+0x2950, reached via GetEnvironment().)

    // The fixed bank of 9 sound-logic state managers (X360 this+79064). Each slot is filled
    // by CreateStateManagers via StateManager::CreateStateMan(i, this) (null when no leaf is
    // registered for that id). Pinned by name; offset NOT asserted.
    CgsSound::Logic::StateManager* mapStateManagers[KI_NUM_STATE_MANAGERS]; // X360 this+79064

    // Prepare-machine bookkeeping (the BRN machine's stage word, X360 word a1[19775] ==
    // byte +0x4CFC, DISTINCT from the engine base's mePrepareStage @+0x4C84; +0x4D00 ==
    // word a1[19776], the voices/base progress word the console stamps 1/2 -- folded into
    // the stage machine here). Pinned by name; the omitted ~79KB of Voice/state-manager/IO
    // members sit between maTriggerActions and these in the real layout (grow-in).
    s32  meBrnPrepareStage;
    bool mbConstructed;
    bool mbPrepared;

    // The "Resource Registrar" CPU monitor handle (X360 word a1[19826] == byte
    // +0x4D48; Prepare case 0 registers it).
    s32  miResourceRegistrarMonitor;

    // DWARF (the dump's PreUpdateOutput member; X360 this+20144 == +0x4EB0 -- the
    // block SoundLogicModule::PreUpdate @0x826E1F10 publishes). By-value embed;
    // pinned by name (host widths differ -- see the header LAYOUT NOTE).
    BrnSound::Module::Io::PreUpdateOutput mPreUpdateOutput;

    // The freed-stream-buffer voice-id list (X360 root+0x5460 == this+0x51E0,
    // directly after mPreUpdateOutput): RootSoundModule::Update @0x826FB238
    // AppendArray's the playback output buffer's freed list into it each frame
    // (the ids of stream voices whose buffers completed their grace scrub).
    // DWARF member name un-dumped; held by usage. Same element type as the
    // playback FreedBuffersArray so AppendArray matches instantiations.
    CgsSound::Playback::Module::Io::OutputBuffer::FreedBuffersArray mFreedStreamBufferIds;
};

} // namespace Module
} // namespace BrnSound

#endif // BRN_SOUND_MODULE_BRN_SOUND_LOGIC_MODULE_H
