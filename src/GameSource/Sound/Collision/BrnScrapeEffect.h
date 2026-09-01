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
//
// MapScrapeToMaterial() @ 0x8269EC18 is DECLARED (DWARF-authoritative private `u8 ...
// const`) but its body is PARTIAL/deferred. Both original semantic blockers are resolved
// (CgsEntityId.h is authoritatively 8/14/10; RootInputBuffer::GetPlayerActiveRaceCarIndex
// is committed), so the reconstruction is understood. It stays deferred only because a
// faithful body must co-include the StateManager RTTI subtree (SoundLogicModule ->
// CgsStateManager.h) alongside ScrapeEffect's effect RTTI subtree (BrnEffectObject.h ->
// CgsEffectBase.h); those two subtrees define incompatible CgsSound::Logic::ClassTypeInfo<T>
// templates -> a pre-existing C2953 ODR fork. Unblock after a tree-wide ClassTypeInfo
// unification (out of scope here); the declared-not-defined method links fine meanwhile.
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
struct CollisionState;

// The per-scrape descriptor MapScrapeToMaterial reads (maObjectId pair); full home
// is BrnCollisionDataStructures.h.
struct ScrapeInfo;

// BrnScrapeEffect.h:36 (DWARF). Reuses the committed BrnEffectObject base BY NAME and
// embeds a CgsSound::Logic::VoiceWrapper.
struct ScrapeEffect : public BrnSound::Logic::BrnEffectObject
{
    ScrapeEffect();
    virtual ~ScrapeEffect();   // BrnScrapeEffect.cpp:73 (DWARF virtual); anchor in .cpp

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const override;
    virtual const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 auAllocator);

    virtual s32 GetController(s32 aiIndex) override;
    virtual void AttachController(CgsSound::Logic::EffectBase* apController) override;
    virtual bool Attach() override;
    virtual void UpdateParams(f32 afDeltaTime) override;
    virtual void ProcessUpdate() override;
    virtual bool Detach() override;

private:
    // @ 0x8269EC18 (DWARF BrnScrapeEffect.h:158: uint8_t MapScrapeToMaterial(const
    // ScrapeInfo&) const; private). Classify a scrape into a material bucket, but only
    // when the player's own race-car is one of the two scraping objects (EntityId
    // owner==1 + entity index == player slot). The OTHER object's owner byte then
    // selects the material (1 -> 1, 2 -> 2, else 0). Body in BrnScrapeEffect.cpp.
    u8 MapScrapeToMaterial(const ScrapeInfo& lScrapeInfo) const;
    f32 GetIntensity(CollisionState* apState, const ScrapeInfo& arScrapeInfo);
    f32 GetGain() const;
    f32 GetPitch() const;

public:
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
