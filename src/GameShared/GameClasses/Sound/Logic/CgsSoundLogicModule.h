#ifndef CGS_SOUND_LOGIC_CGSSOUNDLOGICMODULE_H
#define CGS_SOUND_LOGIC_CGSSOUNDLOGICMODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (range-guarded operator++)

// =============================================================================
// CgsSound::Logic::Module -- the sound-logic module.
//   GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h (DWARF home) +
//   GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. MINIMAL-SLICE NOTE: the Module class
// is a large keystone (Prepare/Release state machines, the VariableEventQueue, the
// Environment, etc.). The surfaces modelled here are the constructor (X360 @0x827E0FD0,
// skeleton sub-object construction), the prepare-stage enum, and its post-increment
// operator (X360 @0x82681D30), which the prepare state machine walks. The rest of the
// class body is left to its full owning keystone TU.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// =============================================================================
// CgsSound::Logic::Module -- the sound-logic subsystem module (the keystone class).
//
// X360 ctor @0x827E0FD0 (CgsSound::Logic::Module::Module) proves the layout shape: a
// CgsModule subsystem-module base (base-most vtable @+0x000, the two base read/write
// RWMutex locks @+0x010 and @+0x118), then -- after the most-derived vtable hand-off --
// the embedded sub-objects:
//   +0x000   base-most (CgsModule) vtable            (off_820CE500)
//   +0x010   base read/write lock A                  (EA::Thread::RWMutex)
//   +0x118   base read/write lock B                  (EA::Thread::RWMutex)
//   +0x000   most-derived (Module) vtable            (off_820D00E0)
//   +0x238   mPlaybackModule  (CgsSound::Playback::Module::Module)
//   +0x29A0  mMicrophoneSystem (CgsSound::Logic::MicrophoneSystem)
//   +0x2C30  the embedded Nicotine::IDynamicMixer-derived mixer sub-object, whose vtable
//            the Module overrides @+0x2C30 with off_820CDC40 (a Module-owned mixer vtable);
//            +0x2C50 (mixer-base +0x20) cleared to 0
//   +0x2C74  trailing byte flag (stb of 0)
//
// SKELETON NOTE: the full ~0x2C75-byte layout is dominated by the embedded
// Playback::Module (~0x2768 bytes) and the IDynamicMixer-derived mixer, neither of which
// is modelled here as a named member. Like the sibling subsystem-module ctors
// (CgsGui::GuiModule -- whose `u8 maHead[0x1B400]` footprint idiom this follows --
// BrnDirector::DirectorModule), the ctor addresses every written location by its
// authoritative X360 byte offset over a raw view of `this`, constructing each owned
// sub-object in place. (2026-08-25 wave 5: the class now RESERVES its console-extent
// footprint -- it used to be sizeof==1, so any `new Module()`/embed would have taken
// ~11KB of out-of-bounds writes from the ctor. FLAG, inherent to this pattern: host
// sub-objects are constructed at CONSOLE offsets; each must fit its console gap --
// MicrophoneSystem does exactly (0x290 == its static_asserted sizeof), the RWMutexes
// fit their 0x108/0x120 gaps, and the embedded Playback::Module's HOST sizeof must
// stay <= its 0x2768 console span, to be re-checked when that keystone grows.)
// =============================================================================
class Module
{
public:
    // The Module's prepare-stage enum. The X360 prepare state machine walks the stages
    // with the post-increment operator below, range-guarding the stepped-to stage against
    // the final eModulePrepareDone stage (== 6, the bound the X360 asserts at
    // CgsSoundLogicModule.h:277: "leEnumIndex <= Module::eModulePrepareDone").
    enum EPrepareState
    {
        eModulePrepareStage0 = 0,
        eModulePrepareStage1 = 1,
        eModulePrepareStage2 = 2,
        eModulePrepareStage3 = 3,
        eModulePrepareStage4 = 4,
        eModulePrepareStage5 = 5,
        eModulePrepareDone   = 6
    };

    // X360 @0x827E0FD0. Install the base + most-derived vtables, build the two base
    // read/write locks, construct the embedded Playback::Module / MicrophoneSystem /
    // mixer sub-objects, override the mixer vtable, and clear the trailing flags.
    Module();

private:
    // The console-extent opaque footprint (0x2C75 written extent, padded to 8).
    // Every ctor write lands inside this span; grow into named members as the
    // keystone slices land (GuiModule maHead precedent).
    u8 mauModuleImage[0x2C78];
};

// Post-increment over the Module prepare stages. X360 @0x82681D30 (DWARF cites the
// assert at CgsSoundLogicModule.h:277): saves the old stage, increments in place, then
// range-guards the stepped-to stage with the project CGS_ASSERT. The X360 fires the
// assert when the incremented stage exceeds Module::eModulePrepareDone (i.e. > 6) and
// returns the SAVED OLD stage in every path -- the guard is a non-gating tripwire.
//   asm: lwz r31,0(r3); addi r11,r31,1; cmpwi r11,6; stw r11,0(r3); ble skip;
//        <Begin/Fire("leEnumIndex <= Module::eModulePrepareDone",...,277)/End>; mr r3,r31
// POST-increment (the trailing dummy `int` Hex-Rays drops as "unused"): it returns the
// SAVED OLD value, the post-increment contract. Caller: Module::Prepare @0x826C42A8.
Module::EPrepareState operator++(Module::EPrepareState& leEnumIndex, int);

}
}

#endif // CGS_SOUND_LOGIC_CGSSOUNDLOGICMODULE_H
