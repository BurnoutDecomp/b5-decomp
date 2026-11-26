#include "GameSource/Sound/Vehicles/Environment/BrnAmbienceControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.h"
#include "GameSource/Sound/BrnMixerData.h"                         // ECameraModes
#include "GameSource/Sound/Global/BrnCameraControl.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"

// ARTIST compiled this file through the GameSource unity file; its asserts carry this path.
#define BRN_AMBIENCEEFFECT_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Sound/Vehicles/Environment/BrnAmbienceEffect.cpp"

// =============================================================================
// BrnSound::Vehicles::Environment::AmbienceControl -- out-of-line bodies.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

bool KB_AMBIENCE_DRAW_REGION = false; // ARTIST byte_82FFB8BF

// The unit's tunable data (ARTIST .data 0x82F2CDF8..0x82F2CE08) has internal linkage: ARTIST
// addresses neighbours through one anchor (SetupLoadData 0x826E6128 `addi r11,-0x3208` then
// lwz 4/0; SelectSpecialAmbience 0x826B991C `addi r30,-0x3200` then lwz 4/0), which the
// compiler only does for file-local data.
// The streamed 2D ambience map. DWARF BrnAmbienceEffect.cpp:45/46 `const char *` (the pointers
// themselves are data: ARTIST 0x82F2CDF8 / 0x82F2CDFC).
static const char* KPC_AMBIENCE_MAP_FILE  = "sound\\regions\\ambiences.dat";
static const char* KPC_AMBIENCE_MAP_ASSET = "Ambiences";

// Region values the control substitutes for the map byte.
static const u8 KU8_TRAFFIC_AMBIENCE       = 18;
static const u8 KU8_HEAVY_TRAFFIC_AMBIENCE = 19;
static const u8 KU8_TUNNEL_AMBIENCE        = 20;

// More live traffic entities than these select the traffic beds. DWARF
// BrnAmbienceEffect.cpp:127/128 declares both as plain (tunable) `int32_t`; ARTIST reads them
// from .data (0x82F2CE00 = 14, 0x82F2CE04 = 22) in SelectSpecialAmbience 0x826B9920/0x826B9948.
static s32 KI_TRAFFIC_THRESHOLD       = 14;
static s32 KI_HEAVY_TRAFFIC_THRESHOLD = 22;

// Seconds between two region selections. DWARF cpp:131 plain `float32_t`, ARTIST 0x82F2CE08.
static f32 KF_AMBIENCE_UPDATE_TIME = 5.0f;

// The world rectangle the ambience map covers (the same Paradise City rectangle the
// district map uses): origin and size in x/z.
static const Vector2 KV_AMBIENCE_MAP_WORLD_ORIGIN( -4208.0f, -3846.0f );
static const Vector2 KV_AMBIENCE_MAP_WORLD_SIZE( 8270.0f, 6101.0f );

// ---------------------------------------------------------------------------
// AmbienceControl::CreateObject(u32)   (the factory hook)
// Tagged allocation now dispatches through the shared sound MemBase allocator.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* AmbienceControl::CreateObject(u32 luType)
{
    if (luType == 0)
        return new (sTypeInfo.typeName, false) AmbienceControl();
    else
        return new (sTypeInfo.typeName, true) AmbienceControl();
}

// Controller slot 0 is the enclosure control.
s32 AmbienceControl::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 10 : -1;
}

void AmbienceControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    // ARTIST 0x82686120: no null test; a switch whose default asserts.
    switch (apController->GetEffectID())
    {
    case 10:
        mpEnclosureControl = static_cast<EnclosureControl*>(apController);
        break;
    default:
        CGS_ASSERT_AT(false, "Unexpected control.", BRN_AMBIENCEEFFECT_CPP_FILE, 190);
        break;
    }
}

// ---------------------------------------------------------------------------
// AmbienceControl::UpdateParams
//
// The map is sampled every tick at the camera microphone's x/z (the console loads the
// current microphone matrix directly; the accessor adds only a validity assert). Until the update timer
// passes KF_AMBIENCE_UPDATE_TIME the region is re-pushed unchanged; then the timer
// restarts and a special ambience (tunnel / traffic) wins over the map byte. An
// off-map sample leaves the region alone. byte_82FFB8BF queues the original 2D map
// coordinate and traffic-count overlay after the region state update.
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
    const Vector2 lListener2d( lrListener.GetX().GetFloat(), lrListener.GetZ().GetFloat() );
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
    }

    if (KB_AMBIENCE_DRAW_REGION)
        DrawDebug(lrListener);
}

// ARTIST 0x8269B318 / DecFIGS BrnAmbienceEffect.h:190. The map point is converted
// to the 256-cell grid without clamping, then the current region and traffic count
// are queued through the 2D DebugRender path.
void AmbienceControl::DrawDebug(Vector3 lrPosition) const
{
    BrnSound::Module::SoundLogicModule* lpLogicModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    const BrnSound::Module::Io::RootInputBuffer* lpInputBuffer =
        lpLogicModule->GetBrnInputStructure();
    CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer");
    if (!lpInputBuffer)
        return;

    const BrnSound::Module::Io::RootInputBuffer::TrafficSoundOutputInterface& lrTraffic =
        lpInputBuffer->GetTrafficOutputInterface();

    CgsDev::DebugInterface lDebugInterface;
    CgsDev::DebugRender& lrRender = lDebugInterface.Get2dRender();
    const CgsDev::RGBA KU_DEBUG_COLOUR = 0xFF00FFA0u; // ARTIST dword_82F2F794
    char lacText[16];

    CgsCore::SPrintf(lacText, static_cast<u32>(sizeof(lacText)), "%d",
                     static_cast<s32>(mRegion.GetCurrent()));
    lrRender.Draw2DText(lacText, 900.0f, 50.0f, 40.0f, KU_DEBUG_COLOUR);

    const f32 lfMapX = ((lrPosition.GetX().GetFloat() - KV_AMBIENCE_MAP_WORLD_ORIGIN.mV.x) /
                        KV_AMBIENCE_MAP_WORLD_SIZE.mV.x) * 256.0f;
    const f32 lfMapY = ((lrPosition.GetZ().GetFloat() - KV_AMBIENCE_MAP_WORLD_ORIGIN.mV.y) /
                        KV_AMBIENCE_MAP_WORLD_SIZE.mV.y) * 256.0f;
    CgsCore::SPrintf(lacText, static_cast<u32>(sizeof(lacText)), "%.0f, %.0f",
                     lfMapX, lfMapY);
    lrRender.Draw2DText(lacText, 850.0f, 100.0f, 25.0f, KU_DEBUG_COLOUR);

    CgsCore::SPrintf(lacText, static_cast<u32>(sizeof(lacText)), "n %d",
                     static_cast<s32>(lrTraffic.GetTrafficEntityCount()));
    lrRender.Draw2DText(lacText, 850.0f, 125.0f, 40.0f, KU_DEBUG_COLOUR);
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
    // DWARF BrnAmbienceEffect.cpp:360; asserts on its unity path at :365 / :379. ARTIST re-reads
    // mpLogicModule for each use (0x826B98C4 / 0x826B98D4).
    CGS_ASSERT_AT(mpEnclosureControl != 0, "mpEnclosureControl", BRN_AMBIENCEEFFECT_CPP_FILE, 365);
    if (mpEnclosureControl->GetTriggerInfo(E_TRIGGER_POSITION_AT_ENTITY)
            .IsTypeActive(BrnTrigger::GenericRegion::E_TYPE_TUNNEL))
    {
        aru8Ambience = KU8_TUNNEL_AMBIENCE;
        return true;
    }

    if (BrnSound::Logic::CameraControl::GetCameraModeFromLogicModule(
            static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule)) ==
        BrnSound::E_CAMERA_MODE_PICTURE_PARADISE)
    {
        return false;
    }

    const BrnSound::Module::Io::RootInputBuffer* lpInputBuffer =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule)->GetBrnInputStructure();
    CGS_ASSERT_AT(lpInputBuffer != 0, "lpInputBuffer", BRN_AMBIENCEEFFECT_CPP_FILE, 379);
    const BrnSound::Module::Io::RootInputBuffer::TrafficSoundOutputInterface& lrTraffic =
        lpInputBuffer->GetTrafficOutputInterface();
    if (lrTraffic.GetTrafficEntityCount() > KI_HEAVY_TRAFFIC_THRESHOLD)
    {
        aru8Ambience = KU8_HEAVY_TRAFFIC_AMBIENCE;
        return true;
    }
    if (lrTraffic.GetTrafficEntityCount() > KI_TRAFFIC_THRESHOLD)
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
// ARTIST 0x82F2F798: {0x10100, "AmbienceControl", BrnEffectControl, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> AmbienceControl::sTypeInfo(
    0x10100, "AmbienceControl", &BrnSound::Logic::BrnEffectControl::sTypeInfo, &AmbienceControl::CreateObject);

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* AmbienceControl::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* AmbienceControl::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* AmbienceControl::GetTypeName() const
{
    return sTypeInfo.typeName;
}

AmbienceControl::~AmbienceControl()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
