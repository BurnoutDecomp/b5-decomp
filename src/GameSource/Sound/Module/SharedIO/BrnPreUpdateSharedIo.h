#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h
//
// Canonical (DWARF) home for the BrnSound::Module::Io pre-update shared-IO types
// (BrnPreUpdateSharedIo.h). MINIMAL slice: it currently homes only
//   * AudioEffectsMessageQueue -- DWARF BrnPreUpdateSharedIo.h:56, verbatim an
//     empty subclass of CgsModule::VariableEventQueue<128,16>. Pointer-free
//     (inline buffer + s32 cursors), so its host sizeof equals the X360 0x90.
// PreUpdateOutput (DWARF :149) is currently sliced in
// GameSource/Sound/Module/BrnRootSoundModuleIo.h (which documents this file as
// the canonical home); it migrates here when its own TU lands -- do NOT define a
// second copy.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<128,16>

namespace BrnSound
{
namespace Module
{
namespace Io
{
    enum eEffectsMessageTypes
    {
        E_EFFECTS_MESSAGE_TYPES_INVALID = 0,
        E_EFFECTS_MESSAGE_TYPES_POP = 1,
        E_EFFECTS_MESSAGE_TYPES_VOICEOVER_FINISHED = 2,
        E_EFFECTS_MESSAGE_TYPES_COUNT = 3
    };

    template <eEffectsMessageTypes teEventType>
    struct AudioEffectsMessageEvent : public CgsModule::Event
    {
        eEffectsMessageTypes GetEventType() const { return teEventType; }
    };

    struct PopEffectsMessage
        : public AudioEffectsMessageEvent<E_EFFECTS_MESSAGE_TYPES_POP>
    {
        PopEffectsMessage() : muRaceCarID(0), mfIntensity(0.0f) {}
        void Construct(u8 auRaceCarID, f32 afIntensity)
        {
            muRaceCarID = auRaceCarID;
            mfIntensity = afIntensity;
        }

        u8 muRaceCarID;
        f32 mfIntensity;
    };

    // DWARF BrnPreUpdateSharedIo.h:56 -- verbatim empty subclass.
    struct AudioEffectsMessageQueue : public CgsModule::VariableEventQueue<128, 16>
    {
    };
}
}
}
