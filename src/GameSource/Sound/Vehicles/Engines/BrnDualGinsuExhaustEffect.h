#ifndef BRN_SOUND_VEHICLES_ENGINES_DUAL_GINSU_EXHAUST_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_DUAL_GINSU_EXHAUST_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Vehicles/Engines/BrnDualGinsuEffect.h"   // DualGinsuEffect base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"     // CgsSound::Logic::VoiceWrapper member (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::DualGinsuExhaustEffect
//   GameSource/Sound/Vehicles/Engines/BrnDualGinsuExhaustEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF (BrnDualGinsuEffect.h:70):
//   DualGinsuExhaustEffect : public DualGinsuEffect
// The exhaust-side dual-Ginsu engine sound EFFECT OBJECT. It adds a reverse-whine voice
// + its own construction state enum over the DualGinsuEffect base.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across the 32/64 pointer boundary.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct DualGinsuExhaustEffect : public DualGinsuEffect
{
    // DWARF BrnDualGinsuEffect.h (DualGinsuExhaustEffect::enumeration meState). The
    // construction/lifecycle state; the ctor seeds it to E_CONSTRUCTED (0).
    enum EState
    {
        E_CONSTRUCTED          = 0,
        E_ATTACHING_BASE_CLASS = 1,
        E_CONSTRUCTING_VOICE   = 2,
        E_ATTACHED             = 3,
        E_DETACHING_BASE_CLASS = 4,
        E_DETACHED             = 5,
        E_STATE_COUNT          = 6,
    };

    DualGinsuExhaustEffect();           // @ 0x826E0E90
    virtual ~DualGinsuExhaustEffect();  // anchor for the vector deleting destructor @ 0x826EC000

    virtual s32 GetController(s32 aiSlot); // @ 0x82684F88
    virtual void AttachController(CgsSound::Logic::EffectBase* apController); // @ 0x82684FC0
    virtual bool Attach(); // @ 0x826F2690
    virtual bool Detach(); // @ 0x826F27C8
    virtual void ProcessUpdate(); // @ 0x826FCDA0

    // @ 0x826E4178 -- RTTI factory hook. Returns the +4 IResourceRequester base view.
    static BrnSound::Logic::IResourceRequester* Creat( s32 aiFlavour );

    EState                        meState;             // h:319 (@ +0x2EC, default E_CONSTRUCTED)
    CgsSound::Logic::VoiceWrapper mReverseWhineVoice;  // h:321 (@ +0x2F0)
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_DUAL_GINSU_EXHAUST_EFFECT_H
