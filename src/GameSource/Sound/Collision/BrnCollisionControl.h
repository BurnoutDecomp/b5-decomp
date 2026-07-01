#ifndef BRN_SOUND_LOGIC_COLLISION_BRN_COLLISION_CONTROL_H
#define BRN_SOUND_LOGIC_COLLISION_BRN_COLLISION_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl base (BY NAME)

// =============================================================================
// BrnSound::Logic::Collision::CollisionControl
//   GameSource/Sound/Collision/BrnCollisionControl.h (DWARF home) +
//   GameSource/Sound/Collision/BrnCollisionControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// CollisionControl is the collision sound-logic EFFECT CONTROL (the control-side
// counterpart of the CollisionEffect object). DWARF (BrnCollisionControl.h:38):
//   struct CollisionControl : public BrnSound::Logic::BrnEffectControl
// so it multiply-inherits the engine effect-control base (primary vptr @ this+0) and
// IResourceRequester (sub-object vptr @ this+4) via the committed BrnEffectControl,
// reused BY NAME.
//
// This TU's recon'd function set is exactly two entries:
//   CreateObject(u32)              @ 0x826D34B0  (the RTTI factory hook)
//   `vector deleting destructor'   @ 0x826BD4D8  (compiler-synthesised; forwards to
//        the ~CollisionControl anchor -- no separate hand-written body)
//
// FLAG (shape vs full surface): this is a MINIMAL home. The full DWARF member set
// (mpCollision3DControl, mpCollisionState, mScrapeInfo (DataPoint<ScrapeInfo>),
// mbCollisionFinished) and the Attach/UpdateParams/GetController/AttachController/
// RTTI surface are DEFERRED to their own recon slices; only the inheritance spine, the
// CreateObject factory and the virtual dtor are materialised. Matching the committed
// ExplosionEffect / EffectControl minimal-home convention, deferred members are NOT
// fabricated as concrete typed fields here.
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

// BrnCollisionControl.h:38 (DWARF). Reuses the committed BrnEffectControl dual base
// BY NAME. The two leaf vptrs the X360 ctor installs are produced structurally by the
// dual-base + virtual-destructor declaration.
struct CollisionControl : public BrnSound::Logic::BrnEffectControl
{
    CollisionControl() {}
    virtual ~CollisionControl();   // out-of-line anchor (empty); vector deleting
                                   // destructor @ 0x826BD4D8 forwards to it

    // BrnCollisionControl.cpp:32 -- the RTTI factory hook (@ 0x826D34B0). STATIC (the
    // createObject function pointer seeded into ClassTypeInfo<EffectControl>::
    // mpfnCreateObject); returns the EffectControl* base view.
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );

    // FLAG: DWARF members mpCollision3DControl (Collision3DControl*), mpCollisionState
    // (CollisionState*), mScrapeInfo (DataPoint<ScrapeInfo>), mbCollisionFinished, and
    // the Attach/UpdateParams/GetController/AttachController RTTI surface are DEFERRED
    // to their own recon slices -- NOT fabricated as concrete typed members here.
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_BRN_COLLISION_CONTROL_H
