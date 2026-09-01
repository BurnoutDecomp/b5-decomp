#include "GameSource/Sound/Collision/BrnCollisionControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
#include "GameSource/Sound/Collision/BrnCollisionEffect.h"
#include "GameSource/Sound/Collision/BrnCollisionState.h"

// =============================================================================
// BrnSound::Logic::Collision::CollisionControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF (BrnCollisionControl.h:38):
//   struct CollisionControl : public BrnSound::Logic::BrnEffectControl
//
// This TU's recon'd function set is exactly two entries:
//   CreateObject(u32)              @ 0x826D34B0
//   `vector deleting destructor'   @ 0x826BD4D8
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

CollisionControl::CollisionControl()
    : BrnSound::Logic::BrnEffectControl()
    , mpCollision3DControl(nullptr)
    , mpCollisionState(nullptr)
    , mScrapeInfo()
    , mbCollisionFinished(false)
{
}

// ---------------------------------------------------------------------------
// CollisionControl::CreateObject(u32)  @ 0x826D34B0   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(176, "CollisionControl", 1) ) return new'd ctor+4; }
//   else      { if ( MemBase::operator new(176, "CollisionControl", 0) ) return new'd ctor+4; }
//   return 0;
//
// The X360 allocates a 176-byte (0xB0) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "CollisionControl" and inline-constructs a
// CollisionControl. Both arms call the SAME size+ctor; the `a1` argument only selects
// the operator-new flavour (0/1). The returned pointer is the constructed object
// upcast to CgsSound::Logic::EffectControl* (the compiler synthesises the base-ptr
// adjustment for `new CollisionControl()`).
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) (off_82FFB954 sound allocator not homed in this group), so a faithful
// placement-new is not yet expressible. This uses the host global `new`; the
// observable result -- a constructed CollisionControl* (or null) handed back as the
// base EffectControl* -- matches. Replace with the sound-allocator placement-new once
// MemBase::operator new is homed. The 0xB0 size is documentation of the X360 inline
// ctor; the zero-init is done by CollisionControl's (deferred) default constructor.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* CollisionControl::CreateObject( u32 /*luType*/ )
{
    return new CollisionControl();
}

// ---------------------------------------------------------------------------
// ~CollisionControl  (the out-of-line anchor the X360 `vector deleting destructor'
// @ 0x826BD4D8 forwards to). The vector deleting destructor is MSVC's compiler-
// synthesised thunk: it chains this virtual destructor and, when bit0 of its flags
// arg is set, frees the object through the global sound MemBase allocator
// (off_82FFB954, vtable slot +0x14). The observable member teardown lives in the
// inherited ~BrnEffectControl base chain (which settles meAttachState/meDetachState/
// mbResourcesReady + the dual-base vptrs), so this leaf body is empty. It exists so
// the class has a defined key function (the vtable emission point), mirroring the
// committed CollisionStateManager / CollisionEffect precedent; the deleting-destructor
// thunk + the delete-half are re-synthesised by the host toolchain from this virtual
// destructor + operator delete (no fabricated allocator).
// ---------------------------------------------------------------------------
CollisionControl::~CollisionControl()
{
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
CollisionControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo(
        0x50000, "CollisionControl",
        CgsSound::Logic::EffectControl::GetStaticTypeInfo(),
        &CollisionControl::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
CollisionControl::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* CollisionControl::GetTypeName() const
{
    return "CollisionControl";
}

s32 CollisionControl::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 1 : -1;
}

void CollisionControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != nullptr, "lpController");
    if (!apController)
        return;

    if (apController->GetEffectID() == 1)
        mpCollision3DControl = static_cast<Collision3DControl*>(apController);
    else
        CGS_ASSERT(false, "Cound't attach controller");
}

// ARTIST @0x826BD580.
bool CollisionControl::Attach()
{
    CgsSound::Logic::EffectBase::Attach();
    mpCollisionState = static_cast<CollisionState*>(GetStateBase());
    CGS_ASSERT(mpCollisionState != nullptr, "mpCollisionState");
    CGS_ASSERT(mpCollision3DControl != nullptr, "mpCollision3DControl");
    if (!mpCollisionState || !mpCollision3DControl)
        return false;

    mbCollisionFinished =
        mpCollisionState->GetLifetime().GetCurrent() == CollisionState::E_SCRAPE;

    CgsSound::Logic::MicrophoneSystem& lrMicrophones =
        GetLogicModule()->GetEnvironment().GetMicrophoneSystem();
    const Matrix44Affine& lrCameraTransform =
        lrMicrophones.GetMicrophone(CgsSound::Logic::MicrophoneSystem::E_MIC_CAMERA,
                                    CgsSound::Logic::MicrophoneSystem::E_PLAYER_1)
            ->GetMicrophoneMatrix();
    mpCollision3DControl->AttachTransform(&lrCameraTransform);
    mpCollision3DControl->AttachEmitterPosition(
        &mpCollisionState->GetOutputCollision().mPosition);
    return true;
}

// ARTIST @0x826F83E8. Keep the two scrape samples in lockstep with the attached
// collision state and release a completed collision once its scrape has ended or
// has not been refreshed for half a second.
void CollisionControl::UpdateParams(f32 /*afDeltaTime*/)
{
    if (!mpCollisionState)
        return;

    const ScrapeInfo& lrScrape =
        mpCollisionState->GetOutputCollision().mScrapeInfo;
    mScrapeInfo.Update(lrScrape);

    const CollisionState::ELifetime leLifetime =
        mpCollisionState->GetLifetime().GetCurrent();
    if (mbCollisionFinished &&
        (leLifetime != CollisionState::E_SCRAPE ||
         mpCollisionState->GetCurrentTime() - lrScrape.mfTimeStamp > 0.5f))
    {
        mpCollisionState->Detach();
    }
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const
    gpCollisionControlReg =
        CgsSound::Logic::EffectControl::AddToClassTypeInfoArray(
            CollisionControl::GetStaticTypeInfo());

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
