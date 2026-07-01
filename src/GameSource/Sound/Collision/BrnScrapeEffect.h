#ifndef BRN_SOUND_LOGIC_COLLISION_BRN_SCRAPE_EFFECT_H
#define BRN_SOUND_LOGIC_COLLISION_BRN_SCRAPE_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"          // BrnSound::Logic::BrnEffectObject (base)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"           // CgsSound::Logic::VoiceWrapper (member)

// =============================================================================
// BrnSound::Logic::Collision::ScrapeEffect
//   GameSource/Sound/Collision/BrnScrapeEffect.h (DWARF home) +
//   GameSource/Sound/Collision/BrnScrapeEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// DWARF (BrnScrapeEffect.h:36): struct ScrapeEffect : public BrnSound::Logic::
// BrnEffectObject. The scrape-audio effect leaf; it embeds a VoiceWrapper (mScrapeVoice)
// and links to the collision control/3D-control.
//
// This TU SHIPS the scalar deleting destructor:
//   `scalar deleting destructor'  @ 0x826E8A28  (compiler-synthesised; forwards to the
//        ~ScrapeEffect anchor -- no separate hand-written body)
// MapScrapeToMaterial() @ 0x8269EC18 is BLOCKED (not shipped): it needs a contested
// EntityId bit-layout (the committed CgsEntityId.h is asm-authoritative 8/12/12 but the
// scan requires 14/10) plus a not-yet-declared RootInputBuffer::GetPlayerActiveRaceCarIndex
// seam -- both would require fabrication, so it is DEFERRED to its own recon slice.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// Opaque forward decls (linked BY POINTER only; homed in their own headers).
struct Collision3DControl;
struct CollisionControl;

// BrnScrapeEffect.h:36 (DWARF). Reuses the committed BrnEffectObject base BY NAME and
// embeds a CgsSound::Logic::VoiceWrapper. Only the destructor is materialised in this
// slice; the rest of the virtual/method surface is DEFERRED.
struct ScrapeEffect : public BrnSound::Logic::BrnEffectObject
{
    ScrapeEffect() {}
    virtual ~ScrapeEffect();   // BrnScrapeEffect.cpp:73 (DWARF virtual); anchor in .cpp

    // Members (DWARF BrnScrapeEffect.h:105..119, order as listed):
    f32                            mfScrapeStartTimeStamp;      // :105
    f32                            mfParam_AEMS_pitch;          // :107
    f32                            mfParam_AEMS_volume;         // :108
    f32                            mfParam_AEMS_friction_stress;// :109
    f32                            mfParam_AEMS_normal_stress;  // :110
    f32                            mfParam_AEMS_time_scraping;  // :111
    f32                            mfParam_AEMS_material_a;     // :112
    u8                             miParam_AEMS_material_b;     // :113
    f32                            mfParam_AEMS_is_Crashing;    // :114
    Collision3DControl*            mpCollision3DControl;        // :116
    CollisionControl*              mpCollisionControl;          // :117
    CgsSound::Logic::VoiceWrapper  mScrapeVoice;                // :118
    bool                           mbPlaying;                   // :119
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_BRN_SCRAPE_EFFECT_H
