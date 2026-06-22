#ifndef BRN_SOUND_MODULE_BRN_SOUND_LOGIC_MODULE_H
#define BRN_SOUND_MODULE_BRN_SOUND_LOGIC_MODULE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                              // EntityId
#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>
#include "GameSource/GameState/BrnGameActions.h"         // BrnGameState::GameStateModuleIO::SoundTriggerAction (element home)
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h" // BrnSound::Logic::IResourceRequester / ResourceRegistrar (committed base + member type)

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
    class LogicInputBuffer;
    class LogicOutputBuffer;
}

struct SoundLogicModule : public BrnSound::Logic::IResourceRequester
{
    // BrnSoundLogicModule.h:58 (DWARF).
    static const s32 KI_MAX_SOUND_TRIGGER_ACTIONS = 16;

    SoundLogicModule() {}
    virtual ~SoundLogicModule() {}

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
    Io::LogicInputBuffer*  mpBrnLogicInputBuffer;   // BrnSoundLogicModule.h:365
    Io::LogicOutputBuffer* mpBrnLogicOutputBuffer;  // BrnSoundLogicModule.h:366

    Array<BrnGameState::GameStateModuleIO::SoundTriggerAction, 16> maTriggerActions; // h:372 (X360 this+0x4CA0)

    BrnSound::Logic::ResourceRegistrar mResourceRegistrar; // h:383 (X360 this+0x588)
};

} // namespace Module
} // namespace BrnSound

#endif // BRN_SOUND_MODULE_BRN_SOUND_LOGIC_MODULE_H
