#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/matrix44affine_operation.h"

// =============================================================================
// BrnSound::Vehicles::Car3DControl (+ Engine/Exhaust/LeftSide/RightSide3dControl)
// — out-of-line deleting-destructor bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See Brn3dCarPosition.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// The five recon'd deleting destructors share one X360 teardown shape:
//   bl   Attrib::Instance::~Instance(this + 0xB0)  ; destroy mEngineDataAtrib
//   li   r9, 3 ; stw r9, 0x24(this)                ; meDetachState = FINISHED
//   stw  &off_820AA820, 0(this)                    ; primary vptr settle
//   stb  0, 0x2D(this)                             ; control bookkeeping flag = false
//   stw  0, 0x20(this)                             ; mfDeltaTime = 0
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The Attrib::Instance member teardown (mEngineDataAtrib) is produced by the
// inherited ~Brn3DEffectControl chain, which destroys the member via the committed
// Attrib::Instance::~Instance. The remaining stores settle members owned by the
// inherited bases, so each leaf destructor body adds nothing of its own.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 deleting destructor) rather
// than reproducing the raw allocator vtable call.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

Car3DControl::Car3DControl()
    : BrnSound::Logic::Brn3DEffectControl()
    , mpPhysicsControl(nullptr)
{
}

s32 Car3DControl::GetController(s32 aiIndex)
{
    // ARTIST uses the ICF-folded MusicEffect::GetController body @ 0x82685D38.
    return aiIndex == 0 ? 0 : -1;
}

void Car3DControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != nullptr, "lpEffectControl");
    if (!apController)
        return;

    CGS_ASSERT(apController->GetEffectID() == 0, "Cound't attach controller ");
    if (apController->GetEffectID() == 0)
        mpPhysicsControl = static_cast<Engines::PhysicsControl*>(apController);
}

bool Car3DControl::Prepare(CgsSound::Logic::State* apState)
{
    if (!BrnSound::Logic::Brn3DEffectControl::Prepare(apState))
        return false;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    CgsSound::Playback::Environment* lpEnvironment =
        lpModule->GetPlaybackModule().GetEnvironment();
    CGS_ASSERT(lpEnvironment != nullptr, "mpObject");
    mbIsStereo = lpEnvironment &&
        lpEnvironment->GetAudioMode() ==
            CgsSound::Playback::Environment::E_AUDIO_MODE_STEREO;
    return true;
}

bool Car3DControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    AttachEmitterPosition(&mCarPosition);
    return true;
}

void Car3DControl::UpdateParams(f32 afDeltaTime)
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return;

    const BrnSound::Vehicles::VehicleData* lpVehiclePhysicsData =
        mpPhysicsControl->GetRawPhysicsData();
    CGS_ASSERT(lpVehiclePhysicsData != nullptr, "mpVehiclePhysicsData");
    if (!lpVehiclePhysicsData)
        return;

    mCarPosition = rw::math::vpu::TransformPoint(
        lpVehiclePhysicsData->mTransform, GetPositionOffset());
    BrnSound::Logic::Brn3DEffectControl::UpdateParams(afDeltaTime);
}

Vector3 Car3DControl::GetPositionOffset() const
{
    Vector3 lOffset;
    lOffset.SetZero();
    return lOffset;
}

Vector3 Engine3dControl::GetPositionOffset() const
{
    // Both ARTIST constants (external and in-car) are zero vectors.
    return Car3DControl::GetPositionOffset();
}

Vector3 Exhaust3dControl::GetPositionOffset() const
{
    // K_EXHAUST_POSITION_OFFSET and the stereo in-car alternative are zero.
    return Car3DControl::GetPositionOffset();
}

Vector3 LeftSide3dControl::GetPositionOffset() const
{
    return Car3DControl::GetPositionOffset();
}

Vector3 RightSide3dControl::GetPositionOffset() const
{
    return Car3DControl::GetPositionOffset();
}

// ~Car3DControl  @ 0x826CF838  (the X360 `scalar deleting destructor')
Car3DControl::~Car3DControl()
{
}

// ~Engine3dControl  @ 0x826E4D70  (the X360 `vector deleting destructor')
Engine3dControl::~Engine3dControl()
{
}

// ~Exhaust3dControl  @ 0x826E4E18  (the X360 `scalar deleting destructor')
Exhaust3dControl::~Exhaust3dControl()
{
}

// ~LeftSide3dControl  @ 0x826E4EC0  (the X360 `scalar deleting destructor')
LeftSide3dControl::~LeftSide3dControl()
{
}

// ~RightSide3dControl  @ 0x826E4F68  (the X360 `vector deleting destructor')
RightSide3dControl::~RightSide3dControl()
{
}

} // namespace Vehicles
} // namespace BrnSound
