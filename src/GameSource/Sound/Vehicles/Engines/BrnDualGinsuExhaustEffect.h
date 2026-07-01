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
        E_CONSTRUCTED = 0,
    };

    DualGinsuExhaustEffect();           // @ 0x826E0E90
    virtual ~DualGinsuExhaustEffect();  // anchor for the vector deleting destructor @ 0x826EC000

    // @ 0x826E4178 -- RTTI factory hook. Returns the +4 IResourceRequester base view.
    static BrnSound::Logic::IResourceRequester* Creat( s32 aiFlavour );

    EState                        meState;             // h:319 (@ +0x2EC, default E_CONSTRUCTED)
    CgsSound::Logic::VoiceWrapper mReverseWhineVoice;  // h:321 (@ +0x2F0)
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_DUAL_GINSU_EXHAUST_EFFECT_H
