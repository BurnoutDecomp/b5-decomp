#ifndef CGS_SOUND_LOGIC_CGSSOUNDLOGICMODULE_H
#define CGS_SOUND_LOGIC_CGSSOUNDLOGICMODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (range-guarded operator++)
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h" // the REAL base (phase B2)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // VariableEventQueue<8192,16> (mMessageQueue)
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"     // Environment / EnvironmentSpec / ModuleParams
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h" // the embedded Playback::Module::Module

namespace rw { struct IResourceAllocator; }
namespace CgsModule { struct IOBuffer; }

// =============================================================================
// CgsSound::Logic::Module -- the sound-logic module.
//   GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h (DWARF home) +
//   GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX + the DecFIGS DWARF
// (CgsSoundLogicModule.h:41: `Module : public CgsModule::ModuleSingleBuffered`).
//
// (2026-08-25, faithful-audio-engine phase B2 RESHAPE): the wave-5 opaque
// `u8 mauModuleImage[0x2C78]` byte image is RETIRED for the real base + the FULL
// DWARF member list. The console layout closes exactly (byte offsets):
//   +0x000..0x227  the ModuleSingleBuffered base (vtable + the two RWMutexes the
//                  old flattened ctor built at +0x10/+0x118)
//   +0x228 mpLogicInputBuffer     +0x22C mpLogicOutputBuffer   (AttachBuffers)
//   +0x230 muUniqueId (Construct = 0)
//   +0x234 muInstanceIndex (= the static instance counter++, X360 dword_82FFBC14)
//   +0x238 mPlaybackModule        (CgsSound::Playback::Module::Module, 0x2718)
//   +0x2950 mEnvironment          (Logic Environment, 0x320 -- the wave-5 ctor's
//           "MicrophoneSystem @+0x29A0" and "mixer @+0x2C30 / +0x2C50 zero" were
//           THIS member's own embedded MicrophoneSystem (+0x50) and DynamicMixer
//           (+0x2E0) -- the flattened stores collapse into member construction)
//   +0x2C70 mpAllocator           (+ the off_82FFB954 file-scope mirror)
//   +0x2C74 mMessageQueue         (VariableEventQueue<8192,16> -- the wave-5
//           ctor's "trailing +0x2C74 byte" was its count byte)
//   +0x4C84 mePrepareStage        +0x4C88 meReleaseStage (Construct seeds
//           eModuleReleaseDone == 5)
// Prepare stage 5 (eModulePrepareProxies) copies the caller's ModuleParams into
// mEnvironment.mModuleParams (the half-word stores at module +0x2994.. == the
// Environment's +0x44 member).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// the console offsets above are comments, not asserted.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

class Module : public CgsModule::ModuleSingleBuffered
{
public:
    // DWARF CgsSoundLogicModule.h:68 (nested; REAL enumerator names -- the
    // wave-5 invented eModulePrepareStage0..6 spellings are retired). The X360
    // prepare machine walks the stages with the post-increment operator below,
    // range-guarding against eModulePrepareDone (the assert at
    // CgsSoundLogicModule.h:277: "leEnumIndex <= Module::eModulePrepareDone").
    // Stages eModulePrepareEnvironment / eModuleCreateHierarchyBuilder are
    // EMPTY bumps in this build (asm-verified).
    enum EModulePrepareStage
    {
        eModulePrepareStart           = 0,
        eModulePrepareManager         = 1,
        eModuleCreateEnvironment      = 2,
        eModulePrepareEnvironment     = 3,
        eModuleCreateHierarchyBuilder = 4,
        eModulePrepareProxies         = 5,
        eModulePrepareDone            = 6,
    };

    // DWARF CgsSoundLogicModule.h:79 (nested).
    enum EModuleReleaseStage
    {
        eModuleReleaseStart           = 0,
        eModuleDestroyHierarchyBuilder = 1,
        eModuleReleaseEnvironment     = 2,
        eModuleDestroyEnvironment     = 3,
        eModuleReleaseManager         = 4,
        eModuleReleaseDone            = 5,
    };

    // X360 @0x827E0FD0. Base + members construct themselves (the base ctor
    // installs the vtable + the two RWMutexes; mPlaybackModule and mEnvironment's
    // embedded MicrophoneSystem/DynamicMixer run their own ctors -- the wave-5
    // flattened store list IS this member construction; the console's mixer
    // vtable override off_820CDC40 is the DynamicMixer subclass vtable, emitted
    // by the compiler here).
    Module();

    // @ 0x826C4230 (DWARF h). Base Construct + zero muUniqueId + stamp
    // muInstanceIndex from the static counter + clear the buffer pointers +
    // construct the message queue + seed the stages (prepare Start / release
    // Done) + mbIsNewModule.
    void Construct() override;

    // @ 0x826C42A8 (DWARF :82: Prepare(rw::IResourceAllocator*, InputBuffer*,
    // OutputBuffer*, const ModuleParams&)). The stage machine (see the .cpp).
    // The pre-switch prologue runs every call: adopt the allocator (+ the
    // off_82FFB954 mirror), Prepare the message queue, AttachBuffers. A NEW
    // virtual (the base's no-arg Prepare keeps its own slot). The IO params are
    // held as base IOBuffer* until the Logic Io pair is retyped (phase B4/B5).
    virtual bool Prepare(rw::IResourceAllocator* apAllocator,
                         CgsModule::IOBuffer* apInputBuffer,
                         CgsModule::IOBuffer* apOutputBuffer,
                         const ModuleParams& akrModuleParams);
    using CgsModule::ModuleSingleBuffered::Prepare;   // keep the base overload visible

    // @ 0x826EAA50 (DWARF :91: Update(f32, f32, InputBuffer*, OutputBuffer*)).
    // AttachBuffers; lock in(read)/out(write); drain the message queue into
    // Environment::Notify; clear it; Environment::Update(gameDt, simDt);
    // unlock; DetachBuffers.
    virtual void Update(f32 af32GameDt, f32 af32SimDt,
                        CgsModule::IOBuffer* apInputBuffer,
                        CgsModule::IOBuffer* apOutputBuffer);

    // @ 0x826C45A8. Walk the queue, handing each event's header (payload - 4) to
    // Environment::Notify.
    void ProcessMessageQueue(CgsModule::VariableEventQueue<8192, 16>* apQueue);

    // @ 0x8268D3F0 / @ 0x8268D400 (DWARF :116/:119, virtual). Bind / clear the
    // logic IO buffer pointers.
    virtual void AttachBuffers(CgsModule::IOBuffer* apInputBuffer,
                               CgsModule::IOBuffer* apOutputBuffer);
    virtual void DetachBuffers();

    // @ 0x827E1078 (DWARF :109). FLAG (DEFER): declared-only -- its own slice.
    virtual u32 GetUniqueId();

    // DWARF :113 -- the state-manager count the CreateEnvironment stage feeds
    // into the EnvironmentSpec (the vtbl+0x4C dispatch in the Prepare asm). The
    // BASE default is un-attested (the GameSource SoundLogicModule overrides it
    // with its real manager census); FLAG'd 0 default keeps the base
    // instantiable.
    virtual s32 GetNumberOfStates() { return 0; }

    // The embedded playback engine, reachable for the RootSoundModule wiring
    // (console +0x238; the +0x4B8 the root reads is THIS member at root +0x280
    // + 0x238).
    Playback::Module::Module& GetPlaybackModule() { return mPlaybackModule; }
    Environment&              GetEnvironment()    { return mEnvironment; }

protected:
    // ---- members (DWARF order/names; console offsets in the banner) ----
    CgsModule::IOBuffer*     mpLogicInputBuffer;   // +0x228 (DWARF :44, InputBuffer*)
    CgsModule::IOBuffer*     mpLogicOutputBuffer;  // +0x22C (DWARF :47, OutputBuffer*)
    u32                      muUniqueId;           // +0x230 (DWARF :50)
    u32                      muInstanceIndex;      // +0x234 (DWARF :53)
    Playback::Module::Module mPlaybackModule;      // +0x238 (DWARF :57)
    Environment              mEnvironment;         // +0x2950 (DWARF :60)
    rw::IResourceAllocator*  mpAllocator;          // +0x2C70 (DWARF :63)
    CgsModule::VariableEventQueue<8192, 16>
                             mMessageQueue;        // +0x2C74 (DWARF :66, MessageQueue)
    EModulePrepareStage      mePrepareStage;       // +0x4C84 (DWARF :69)
    EModuleReleaseStage      meReleaseStage;       // +0x4C88 (DWARF :72)
};

// Post-increment over the Module prepare stages. X360 @0x82681D30 (DWARF cites the
// assert at CgsSoundLogicModule.h:277): saves the old stage, increments in place, then
// range-guards the stepped-to stage with the project CGS_ASSERT. The X360 fires the
// assert when the incremented stage exceeds Module::eModulePrepareDone (i.e. > 6) and
// returns the SAVED OLD stage in every path -- the guard is a non-gating tripwire.
Module::EModulePrepareStage operator++(Module::EModulePrepareStage& leEnumIndex, int);

// DWARF CgsSoundLogicModule.h:278 -- the release-stage sibling (bound
// eModuleReleaseDone == 5).
Module::EModuleReleaseStage operator++(Module::EModuleReleaseStage& leEnumIndex, int);

}
}

#endif // CGS_SOUND_LOGIC_CGSSOUNDLOGICMODULE_H
