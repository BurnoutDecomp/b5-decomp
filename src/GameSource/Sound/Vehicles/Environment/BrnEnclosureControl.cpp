#include "GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"

// =============================================================================
// BrnSound::Vehicles::Environment::EnclosureControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   ConvertRegionTypeToIndex(int)  @ 0x82685FA0
//   Create(bool)                   @ 0x826D0A30
//   `vector deleting destructor'   @ 0x826B94A8  (-> ~EnclosureControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// ---------------------------------------------------------------------------
// EnclosureControl::ConvertRegionTypeToIndex(int)  @ 0x82685FA0
//
// The X360 lays out the switch as `switch(liRegionType - 25)` over a 6-entry jump table
// (cases 25..30) with everything else (including in-range 19..24, 31) falling to the
// default 19. The `this` pointer is passed but never dereferenced -> pure mapping.
// ---------------------------------------------------------------------------
int EnclosureControl::ConvertRegionTypeToIndex( int liRegionType ) const
{
    CGS_ASSERT(liRegionType >= 19 && liRegionType <= 31,
               "EnclosureControl : Region type out of range.");

    switch ( liRegionType )
    {
    case 25: return 9;
    case 26: return 13;
    case 27: return 8;
    case 28: return 14;
    case 29: return 17;
    case 30: return 15;
    default: return 19;
    }
}

// ---------------------------------------------------------------------------
// EnclosureControl::Create(bool)  @ 0x826D0A30   (the factory)
// Allocates a 0x50 (80) byte block via CgsSound::MemBase::operator new(size, tag,
// flavour) tagged "EnclosureControl" and inline-constructs an EnclosureControl, upcast
// to the EffectObject* base (+4 adjust). The bool arg only selects the operator-new
// flavour (0/1); both arms use the same size + ctor.
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; observable result matches. The 0x50 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* EnclosureControl::Create( bool /*lbFlavour*/ )
{
    return new EnclosureControl();
}

// The shortest gap between two whoosh triggers, in seconds; Attach starts the timer here
// so the first trigger is never suppressed.
static const f32 KF_MIN_TIME_BETWEEN_TRIGGERS = 0.5f;

// Controller slot 0 is the physics control.
s32 EnclosureControl::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 0 : -1;
}

void EnclosureControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController->GetEffectID() == 0, "Unexpected control.");
    if (apController->GetEffectID() == 0)
        mpPhysicsControl = static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
}

// Clears both trigger sets and arms the trigger timer.
bool EnclosureControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    for (s32 liPosition = 0; liPosition < E_TRIGGER_POSITION_COUNT; ++liPosition)
        maTriggerInfo[liPosition].Reset();
    mfTimeSinceTrigger = KF_MIN_TIME_BETWEEN_TRIGGERS;
    return true;
}

// ---------------------------------------------------------------------------
// ~EnclosureControl  @ 0x826B94A8  (anchor for the X360 `vector deleting destructor').
// The observable member teardown -- the dual-base vptr settle + meDetachState/
// mbResourcesReady/meAttachState clears -- is the inherited ~BrnEffectObject chain
// (byte-identical to the sibling SpeedStreamControl @ 0x826BA0A0), so this leaf body is
// empty. The (a2 & 1) allocator-free tail is left to the host toolchain.
// ---------------------------------------------------------------------------
EnclosureControl::~EnclosureControl()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
