#include "GameSource/Sound/Vehicles/Environment/BrnAmbienceControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnvironmentSoundDiag.h"
#include "GameSource/Sound/BrnMixerData.h"                         // ECameraModes
#include "GameSource/Sound/Global/BrnCameraControl.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"

// =============================================================================
// BrnSound::Vehicles::Environment::AmbienceControl -- out-of-line bodies.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// The streamed 2D ambience map.
static const char* const KPC_AMBIENCE_MAP_FILE  = "sound\\regions\\ambiences.dat";
static const char* const KPC_AMBIENCE_MAP_ASSET = "Ambiences";

// Region values the control substitutes for the map byte.
static const u8 KU8_TRAFFIC_AMBIENCE       = 18;
static const u8 KU8_HEAVY_TRAFFIC_AMBIENCE = 19;
static const u8 KU8_TUNNEL_AMBIENCE        = 20;

// More live traffic entities than these select the traffic beds.
static const s32 KI_TRAFFIC_THRESHOLD       = 14;
static const s32 KI_HEAVY_TRAFFIC_THRESHOLD = 22;

// Seconds between two region selections.
static const f32 KF_AMBIENCE_UPDATE_TIME = 5.0f;

// The world rectangle the ambience map covers (the same Paradise City rectangle the
// district map uses): origin and size in x/z.
static const Vector2 KV_AMBIENCE_MAP_WORLD_ORIGIN = { -4208.0f, -3846.0f, 0.0f, 0.0f };
static const Vector2 KV_AMBIENCE_MAP_WORLD_SIZE   = {  8270.0f,  6101.0f, 0.0f, 0.0f };

// ---------------------------------------------------------------------------
// AmbienceControl::CreateObject(u32)   (the factory hook)
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; observable result matches. Mirrors CollisionControl::CreateObject.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* AmbienceControl::CreateObject( u32 /*luType*/ )
{
    return new AmbienceControl();
}

// Controller slot 0 is the enclosure control.
s32 AmbienceControl::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 10 : -1;
}

void AmbienceControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController->GetEffectID() == 10, "Unexpected control.");
    if (apController->GetEffectID() == 10)
        mpEnclosureControl = static_cast<EnclosureControl*>(apController);
}

// ---------------------------------------------------------------------------
// AmbienceControl::UpdateParams
//
// The map is sampled every tick at the camera microphone's x/z (the console loads the
// current microphone matrix directly; the accessor adds only a validity assert). Until the update timer
// passes KF_AMBIENCE_UPDATE_TIME the region is re-pushed unchanged; then the timer
// restarts and a special ambience (tunnel / traffic) wins over the map byte. An
// off-map sample leaves the region alone. The debug region overlay behind
// KB_AMBIENCE_DRAW_REGION (off in the shipped image) is not reproduced.
// ---------------------------------------------------------------------------
void AmbienceControl::UpdateParams(f32 afTimeStep)
{
    const Vector3& lrListener =
        mpLogicModule->GetEnvironment()
            .GetMicrophoneSystem()
            .GetMicrophone(CgsSound::Logic::MicrophoneSystem::E_MIC_CAMERA,
                           CgsSound::Logic::MicrophoneSystem::E_PLAYER_1)
            ->GetMicrophoneMatrix()
            .Pos();
    const Vector2 lListener2d = { lrListener.x, lrListener.z, 0.0f, 0.0f };
    const u8 lu8MapRegion = mMap2d.GetValue(lListener2d);

    mfTimeSinceUpdate += afTimeStep;
    u8 lu8SpecialAmbience = 0;
    if (mfTimeSinceUpdate <= KF_AMBIENCE_UPDATE_TIME)
    {
        mRegion.Update(mRegion.GetCurrent());
    }
    else
    {
        mfTimeSinceUpdate = 0.0f;
        if (SelectSpecialAmbience(lu8SpecialAmbience))
            mRegion.Update(lu8SpecialAmbience);
        else if (lu8MapRegion != CgsWorld::KU_INVALID_WORLD_MAP_VALUE)
            mRegion.Update(lu8MapRegion);

        static s32 siDiagSelect = 0;
        if (SndEnvDiagBudget(siDiagSelect))
            *CgsDev::Log::gpDebugPrint << "[sndenv] ambience-control select map="
                                       << static_cast<s32>(lu8MapRegion) << " region="
                                       << static_cast<s32>(mRegion.GetCurrent())
                                       << " [FLAG PC witness]\n";
    }
}

// ---------------------------------------------------------------------------
// AmbienceControl::Attach
//
// Binds the 2D map over the streamed blob (the resource's main memory plus the blob's
// own header offset, a serialised-file walk), marks the region invalid and arms the
// timer so the first update selects at once.
// ---------------------------------------------------------------------------
bool AmbienceControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    mMap2dResource = GetAsset(KPC_AMBIENCE_MAP_FILE, KPC_AMBIENCE_MAP_ASSET);
    CGS_ASSERT(mMap2dResource.mpResourceMemory != 0, "mMap2dResource.GetResource() != 0");
    const u8* lpMemoryResource = *reinterpret_cast<u8* const*>(mMap2dResource.mpResourceMemory);
    CGS_ASSERT(lpMemoryResource != 0,
               "mMap2dResource.GetResource()->GetMemoryResource() != 0");

    const void* lpMapBlob =
        lpMemoryResource + *reinterpret_cast<const u32*>(lpMemoryResource + 4);  // serialised ambiences.dat header
    mMap2d.Construct(lpMapBlob, KV_AMBIENCE_MAP_WORLD_ORIGIN, KV_AMBIENCE_MAP_WORLD_SIZE);

    mRegion.Flush(KU8_INVALID_REGION);
    mfTimeSinceUpdate = KF_AMBIENCE_UPDATE_TIME;
    return true;
}

// Requests the ambience map.
void AmbienceControl::SetupLoadData()
{
    LoadAsset(KPC_AMBIENCE_MAP_FILE, KPC_AMBIENCE_MAP_ASSET,
              BrnSound::Logic::ResourceRegistrar::E_DATA);
}

// ---------------------------------------------------------------------------
// AmbienceControl::SelectSpecialAmbience
//
// A tunnel at the car wins; otherwise, except in the picture-paradise camera, heavy and
// then light traffic (by live traffic-entity count) select the traffic beds.
// ---------------------------------------------------------------------------
bool AmbienceControl::SelectSpecialAmbience(u8& aru8Ambience) const
{
    CGS_ASSERT(mpEnclosureControl != nullptr, "mpEnclosureControl");
    if (mpEnclosureControl->GetTriggerInfo(E_TRIGGER_POSITION_AT_ENTITY)
            .IsTypeActive(BrnTrigger::GenericRegion::E_TYPE_TUNNEL))
    {
        aru8Ambience = KU8_TUNNEL_AMBIENCE;
        return true;
    }

    BrnSound::Module::SoundLogicModule* lpLogicModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    if (BrnSound::Logic::CameraControl::GetCameraModeFromLogicModule(lpLogicModule) ==
        BrnSound::E_CAMERA_MODE_PICTURE_PARADISE)
    {
        return false;
    }

    const BrnSound::Module::Io::RootInputBuffer* lpInputBuffer =
        lpLogicModule->GetBrnInputStructure();
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer");
    const BrnSound::Module::Io::RootInputBuffer::TrafficSoundOutputInterface& lrTraffic =
        lpInputBuffer->GetTrafficOutputInterface();
    if (static_cast<s32>(lrTraffic.GetTrafficEntityCount()) > KI_HEAVY_TRAFFIC_THRESHOLD)
    {
        aru8Ambience = KU8_HEAVY_TRAFFIC_AMBIENCE;
        return true;
    }
    if (static_cast<s32>(lrTraffic.GetTrafficEntityCount()) > KI_TRAFFIC_THRESHOLD)
    {
        aru8Ambience = KU8_TRAFFIC_AMBIENCE;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ~AmbienceControl  (anchor for the vector deleting destructor).
// Every stored member the console teardown touches (meDetachState/meAttachState/
// mbResourcesReady) is owned by the BrnEffectControl base, so this leaf body adds
// nothing; the allocator-free tail is left to the host toolchain.
// ---------------------------------------------------------------------------
AmbienceControl::~AmbienceControl()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
