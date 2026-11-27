#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// ARTIST compiled this file through the GameSource unity file; its asserts carry this path.
#define BRN_3DCARPOSITION_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Sound/Vehicles/Brn3dCarPosition.cpp"

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

// DWARF Brn3dCarPosition.cpp:20..27: plain (non-const) Vector3 globals, the original's 'POSIITON'
// spelling kept. ARTIST builds each in a dynamic initialiser (0x82C625B0, 0x82C625F0, 0x82C62630,
// 0x82C62670, 0x82C626B8: the three-float aggregate fill, w zeroed), with these values.
Vector3 K_ENGINE_POSIITON_OFFSET(0.0f, 0.0f, 1.0f);           // 0x82FFBDE0
Vector3 K_EXHAUST_POSIITON_OFFSET(0.0f, 0.0f, -1.0f);         // 0x83005FE0
Vector3 K_ENGINE_POSIITON_IN_CAR_OFFSET(0.0f, 0.0f, 4.0f);    // 0x83005F40
Vector3 K_LEFTSIDE_POSIITON_OFFSET(2.0f, 0.0f, 1.0f);         // 0x82FFBE00
Vector3 K_RIGHTSIDE_POSIITON_OFFSET(-2.0f, 0.0f, 1.0f);       // 0x82FFBE10

// ARTIST 0x82F2F690: {0x10070, "Car3DControl", Brn3DEffectControl, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> Car3DControl::sTypeInfo(
    0x10070, "Car3DControl", &BrnSound::Logic::Brn3DEffectControl::sTypeInfo, &Car3DControl::CreateObject);

CgsSound::Logic::EffectControl* Car3DControl::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) Car3DControl();
    else
        return new (sTypeInfo.typeName, true) Car3DControl();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* Car3DControl::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* Car3DControl::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* Car3DControl::GetTypeName() const
{
    return sTypeInfo.typeName;
}

Car3DControl::Car3DControl()
    : BrnSound::Logic::Brn3DEffectControl()
    , mpPhysicsControl(0)
{
}

s32 Car3DControl::GetController(s32 aiIndex)
{
    // ARTIST uses the ICF-folded MusicEffect::GetController body @ 0x82685D38.
    return aiIndex == 0 ? 0 : -1;
}

void Car3DControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    // ARTIST 0x826858D0: no null test; a switch whose default streams the controller id.
    switch (apController->GetEffectID())
    {
    case 0:
        mpPhysicsControl = static_cast<Engines::PhysicsControl*>(apController);
        break;
    default:
        CGS_FAIL_STREAM_AT("Cound't attach controller " << apController->GetId(),
                             BRN_3DCARPOSITION_CPP_FILE, 224);
        break;
    }
}

bool Car3DControl::Prepare(CgsSound::Logic::State* apState)
{
    if (!BrnSound::Logic::Brn3DEffectControl::Prepare(apState))
        return false;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    CgsSound::Playback::Environment* lpEnvironment =
        &lpModule->GetPlaybackModule().GetEnvironment();
    CGS_ASSERT(lpEnvironment != 0, "mpObject");
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
    CGS_ASSERT(mpPhysicsControl != 0, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return;

    const BrnSound::Vehicles::VehicleData* lpVehiclePhysicsData =
        mpPhysicsControl->GetRawPhysicsData();
    CGS_ASSERT(lpVehiclePhysicsData != 0, "mpVehiclePhysicsData");
    if (!lpVehiclePhysicsData)
        return;

    mCarPosition = rw::math::vpu::TransformPoint(
        lpVehiclePhysicsData->mTransform, GetPositionOffset());
    BrnSound::Logic::Brn3DEffectControl::UpdateParams(afDeltaTime);
}

// ARTIST 0x826858A0: the three-float Vector3 fill (DWARF inlines Vector3::Vector3 here).
Vector3 Car3DControl::GetPositionOffset() const
{
    return Vector3(0.0f, 0.0f, 0.0f);
}

// ARTIST 0x8269A860: the bumper camera (CameraState flag bit 4 at state+8, ld 0x140 of the director
// camera) moves the engine to K_ENGINE_POSIITON_IN_CAR_OFFSET. DWARF inlines GetBrnLogicModule,
// GetBrnInputStructure (the BrnSoundLogicModule.h:432 assert) and calls GetDirectorCamera.
Vector3 Engine3dControl::GetPositionOffset() const
{
    BrnSound::Module::SoundLogicModule* lpLogicModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    if (lpLogicModule->GetBrnInputStructure()->GetDirectorCamera()->GetState().IsFlagSet(
            BrnDirector::Camera::CameraState::E_FLAG_BUMPER_CAM))
    {
        return K_ENGINE_POSIITON_IN_CAR_OFFSET;
    }
    return K_ENGINE_POSIITON_OFFSET;
}

// ARTIST 0x8269A910: a stereo exhaust (mbIsStereo, lbz 0xE4) under the bumper camera takes the engine's
// in-car offset; otherwise K_EXHAUST_POSIITON_OFFSET. Two exhaust returns: ARTIST copies the result
// pointer on each path (mr r3,r31 at 0x8269A988 and 0x8269A9A0) and merges only their tails.
Vector3 Exhaust3dControl::GetPositionOffset() const
{
    if (mbIsStereo)
    {
        BrnSound::Module::SoundLogicModule* lpLogicModule =
            static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
        if (lpLogicModule->GetBrnInputStructure()->GetDirectorCamera()->GetState().IsFlagSet(
                BrnDirector::Camera::CameraState::E_FLAG_BUMPER_CAM))
        {
            return K_ENGINE_POSIITON_IN_CAR_OFFSET;
        }
        return K_EXHAUST_POSIITON_OFFSET;
    }
    return K_EXHAUST_POSIITON_OFFSET;
}

Vector3 LeftSide3dControl::GetPositionOffset() const
{
    return K_LEFTSIDE_POSIITON_OFFSET;
}

Vector3 RightSide3dControl::GetPositionOffset() const
{
    return K_RIGHTSIDE_POSIITON_OFFSET;
}

// ~Car3DControl  @ 0x826CF838  (the X360 `scalar deleting destructor')
// (~Car3DControl is implicit: see the header.)

// ~Engine3dControl  @ 0x826E4D70  (the X360 `vector deleting destructor')
// ARTIST 0x82F2F6A0: {0x100D0, "Engine3dControl", Car3DControl, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> Engine3dControl::sTypeInfo(
    0x100D0, "Engine3dControl", &BrnSound::Vehicles::Car3DControl::sTypeInfo, &Engine3dControl::CreateObject);

CgsSound::Logic::EffectControl* Engine3dControl::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) Engine3dControl();
    else
        return new (sTypeInfo.typeName, true) Engine3dControl();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* Engine3dControl::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* Engine3dControl::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* Engine3dControl::GetTypeName() const
{
    return sTypeInfo.typeName;
}

// (~Engine3dControl is implicit: see the header.)

// ~Exhaust3dControl  @ 0x826E4E18  (the X360 `scalar deleting destructor')
// ARTIST 0x82F2F6B0: {0x100C0, "Exhaust3dControl", Car3DControl, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> Exhaust3dControl::sTypeInfo(
    0x100C0, "Exhaust3dControl", &BrnSound::Vehicles::Car3DControl::sTypeInfo, &Exhaust3dControl::CreateObject);

CgsSound::Logic::EffectControl* Exhaust3dControl::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) Exhaust3dControl();
    else
        return new (sTypeInfo.typeName, true) Exhaust3dControl();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* Exhaust3dControl::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* Exhaust3dControl::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* Exhaust3dControl::GetTypeName() const
{
    return sTypeInfo.typeName;
}

// (~Exhaust3dControl is implicit: see the header.)

// ~LeftSide3dControl  @ 0x826E4EC0  (the X360 `scalar deleting destructor')
// ARTIST 0x82F2F6C0: {0x10080, "LeftSide3dControl", Car3DControl, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> LeftSide3dControl::sTypeInfo(
    0x10080, "LeftSide3dControl", &BrnSound::Vehicles::Car3DControl::sTypeInfo, &LeftSide3dControl::CreateObject);

CgsSound::Logic::EffectControl* LeftSide3dControl::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) LeftSide3dControl();
    else
        return new (sTypeInfo.typeName, true) LeftSide3dControl();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* LeftSide3dControl::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* LeftSide3dControl::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* LeftSide3dControl::GetTypeName() const
{
    return sTypeInfo.typeName;
}

// (~LeftSide3dControl is implicit: see the header.)

// ~RightSide3dControl  @ 0x826E4F68  (the X360 `vector deleting destructor')
// ARTIST 0x82F2F6D0: {0x10090, "RightSide3dControl", Car3DControl, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> RightSide3dControl::sTypeInfo(
    0x10090, "RightSide3dControl", &BrnSound::Vehicles::Car3DControl::sTypeInfo, &RightSide3dControl::CreateObject);

CgsSound::Logic::EffectControl* RightSide3dControl::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) RightSide3dControl();
    else
        return new (sTypeInfo.typeName, true) RightSide3dControl();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* RightSide3dControl::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* RightSide3dControl::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* RightSide3dControl::GetTypeName() const
{
    return sTypeInfo.typeName;
}

// (~RightSide3dControl is implicit: see the header.)

} // namespace Vehicles
} // namespace BrnSound
