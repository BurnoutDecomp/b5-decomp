#ifndef BRN_SOUND_VEHICLES_ENGINES_SWEETENERS_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_SWEETENERS_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper members (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"            // CgsSound::Utils::PathLine<2> member (BY NAME)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

// =============================================================================
// BrnSound::Vehicles::Engines::SweetenersEffect
//   GameSource/Sound/Vehicles/Engines/BrnSweetenersEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. SweetenersEffect is the engine
// "sweetener" grain-layer EFFECT OBJECT (DWARF: SweetenersEffect : public
// BrnEffectObject). It layers up to five voices with a per-effect random jitter table.
//
// FLAG (MINIMAL home): the ctor also installs an inner {off_820B3250,0,0} sub-object
// @ +0x158 and zero-/one-inits a set of un-homed leaf scalar fields; those names/types
// (and the +0x158 vtable identity) are un-homed and DECLARATION-DEFERRED (NOT bound to
// the committed CgsSound::Playback::Content -- unproven identity). Only the base (BY
// NAME) + the five embedded VoiceWrappers + the PathLine<2> + the jitter table are
// materialised.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct PhysicsControl;
struct ShiftControl;

struct SweetenersEffect : public BrnSound::Logic::BrnEffectObject
{
    SweetenersEffect();             // @ 0x826CF290
    virtual ~SweetenersEffect();    // anchor for the vector deleting destructor @ 0x826E4B18

    virtual s32  GetController(s32 aiSlot); // @ 0x82685558
    virtual void AttachController(CgsSound::Logic::EffectBase* apController); // @ 0x82685580
    virtual bool Attach(); // @ 0x826FD7A0
    virtual void UpdateParams(f32 afTimeStep); // @ 0x826FCEE8

    // @ 0x826CF5A0 -- RTTI factory hook. Returns the +4 IResourceRequester base view.
    static BrnSound::Logic::IResourceRequester* CreateObjec( u32 luFlavour );

    // ---- ctor-constructed members (BY NAME; DWARF-attested) ----
    CgsSound::Logic::VoiceWrapper mVoice0;   // @ +0x7C
    CgsSound::Logic::VoiceWrapper mVoice1;   // @ +0xCC
    CgsSound::Logic::VoiceWrapper mVoice2;   // @ +0x1D4
    CgsSound::Logic::VoiceWrapper mVoice3;   // @ +0x228
    CgsSound::Logic::VoiceWrapper mVoice4;   // @ +0x28C
    f32                           mafJitterTable[8]; // @ +0x120 (per-effect RNG jitter)
    CgsSound::Utils::PathLine<2u> mPathLine; // @ +0x2DC

    // DWARF BrnSweetenersEffect.h:227,245,246,259. These are the members used by
    // the player-engine mix gate in Attach/UpdateParams. They are pinned by name
    // and sequence on the 64-bit host; the X360 offsets are +0x34, +0x14C,
    // +0x150, and +0x1C0 respectively.
    bool mbEnableSweetners;
    PhysicsControl* mpPhysicsControl;
    ShiftControl* mpShiftingControl;
    CgsSound::Utils::DataPoint<
        BrnWorld::RaceCarEntityModuleIO::EActiveRaceCarEngineState> meRaceCarEngineState;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_SWEETENERS_EFFECT_H
