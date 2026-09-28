#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.h"
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficSoundDiag.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"

#include <algorithm>
#include <cmath>

// =============================================================================
// BrnSound::Logic::Traffic::TrafficStateManager -- out-of-line bodies.
//
// FLAG (replay serialiser not in this tree): while the sound serialiser records, the
// console's UpdateParams mirrors the six nearest entities into it, and while it plays
// back it matches and attaches the recorded entities instead of the live list. This
// tree's SoundLogicModule holds no SoundSerialiser and nothing drives its mode, so only
// the live traffic interface is read -- the console's own path whenever no replay runs.
//
// Not carried: the KI_SPEW_TRAFFIC_MANAGEMENT developer TTY lines ("[Traffic]
// Successfully Culled." / "Failed to Cull for item @ distance of"), gated on a switch
// that is zero in the image and only registered with the debug-variable registry.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

namespace
{
// An entity more than this far above or below the camera microphone is not attached
// (a splat constant the CRT fills from 99.0f).
const f32 K_TRAFFIC_IGNORE_HEIGHT = 99.0f;

VecFloat SplatDistance(f32 lfValue)
{
    const VecFloat lvValue = { lfValue, lfValue, lfValue, lfValue };
    return lvValue;
}

u32 guAttachWitnesses  = 0;
u32 guCullWitnesses    = 0;
u32 guPrepareWitnesses = 0;
u32 guDetachWitnesses  = 0;
} // namespace

// The console constructor runs the StateManager base, installs both vtables, clears
// mbActive / mpAttachedState of the 32 slots and default-constructs the four banks.
TrafficStateManager::TrafficStateManager()
    : BrnStateManager()
{
}

// The four bank Content destructors release their objects in reverse order, then the
// base StateManager tears its content pool down.
TrafficStateManager::~TrafficStateManager()
{
}

// MemBase::operator new(0xCD0, "TrafficStateManager", flavour) + the constructor; the
// argument only picks the operator-new flavour.
CgsSound::Logic::StateManager* TrafficStateManager::CreateObject( u32 /*luType*/ )
{
    return new TrafficStateManager();
}

// Descriptor {3, "TrafficStateManager", StateManager::sTypeInfo, &CreateObject}.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* TrafficStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        3,
        "TrafficStateManager",
        CgsSound::Logic::StateManager::GetStaticTypeInfo(),
        &TrafficStateManager::CreateObject );
    return &sTypeInfo;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpTrafficStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            TrafficStateManager::GetStaticTypeInfo() );

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* TrafficStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* TrafficStateManager::GetTypeName() const
{
    return "TrafficStateManager";
}

// ---------------------------------------------------------------------------
// Prepare (vtable +0x0C): a switch on mePrepareState.
//   0/5 -> 0
//   1   construct the two CSIS interfaces through the AEMS factory, then request the
//       traffic and horn AEMS bundles (ResourcesAreReady constructs their banks);
//   2   wait until both CSIS interfaces exist and all four contents report loaded,
//       then register the "Traffic Cars" CPU monitor;
//   3   PrepareStates(15, 6, 0);
//   4   finished.
// ---------------------------------------------------------------------------
bool TrafficStateManager::Prepare()
{
    switch ( mePrepareState )
    {
    case E_PREPARE_NONE:
    case E_PREPARE_RELEASED:
        mePrepareState = E_PREPARE_NONE;
        // fall through
    case E_PREPARE_BEGIN:
    {
        mePrepareState = E_PREPARE_BEGIN;
        const u32 luAemsFactory = static_cast<u32>( CgsSound::Playback::AemsFactorySkName().GetValue() );
        mEngineCsisInterface.Construct(
            GetLogicModule(), luAemsFactory,
            static_cast<u32>( CgsSound::Playback::Name::MakeHash( "TrafficCsis" ) ) );
        mHornCsisInterface.Construct(
            GetLogicModule(), luAemsFactory,
            static_cast<u32>( CgsSound::Playback::Name::MakeHash( "HornsCsis" ) ) );
        LoadAsset( "sound\\aems\\Traffic_Bank.bundle", nullptr, ResourceRegistrar::E_DATA );
        LoadAsset( "sound\\aems\\patch_bank_horns.bundle", nullptr, ResourceRegistrar::E_DATA );
    }
        // fall through
    case E_PREPARE_UPDATING:
        mePrepareState = E_PREPARE_UPDATING;
        if ( !mEngineCsisInterface.IsCreated() || !mHornCsisInterface.IsCreated() )
            return false;
        if ( !mEngineCsisInterface.IsLoaded() || !mHornCsisInterface.IsLoaded()
             || !mEngineAemsBank.IsLoaded() || !mHornAemsBank.IsLoaded() )
            return false;
        miCpuMonitor = CgsDev::PerfMonCpu::AddMonitor( "Traffic Cars", 14, 0, 1.0, 0, 1 );
        // fall through
    case E_PREPARE_STATES:
        mePrepareState = E_PREPARE_STATES;
        if ( !PrepareStates( KI_TRAFFIC_STATE_EFFECT_MASK, KU_NUMBER_OF_TRAFFIC_CARS, 0 ) )
            return false;
        // fall through
    case E_PREPARE_FINISHED:
        mePrepareState = E_PREPARE_FINISHED;
        // [FLAG PC witness] BRN_TRAFFICSND_DIAG
        if ( TrafficSoundDiagTake( guPrepareWitnesses, 1 ) )
        {
            *CgsDev::Log::gpDebugPrint
                << "[trafficsnd] manager prepared states=" << GetStateObjCount() << "\n";
        }
        return true;
    default:
        return false;
    }
}

// IResourceRequester callback: both traffic bundles resolved -- construct the engine
// and horn AEMS banks through the AEMS factory.
void TrafficStateManager::ResourcesAreReady()
{
    const u32 luAemsFactory = static_cast<u32>( CgsSound::Playback::AemsFactorySkName().GetValue() );
    mEngineAemsBank.Construct(
        GetLogicModule(), luAemsFactory,
        static_cast<u32>( CgsSound::Playback::Name::MakeHash( "Traffic_Bank.abi" ) ) );
    mHornAemsBank.Construct(
        GetLogicModule(), luAemsFactory,
        static_cast<u32>( CgsSound::Playback::Name::MakeHash( "patch_bank_horns.abi" ) ) );
}

void TrafficStateManager::ExitGamePlay()
{
    mEngineAemsBank.Destruct();
    mHornAemsBank.Destruct();
    mEngineCsisInterface.Destruct();
    mHornCsisInterface.Destruct();
}

// ---------------------------------------------------------------------------
// UpdateParams (vtable +0x18). Not gated on the prepare state: the base update runs
// the states, then every active slot takes the fresh copy of its entity from the
// traffic list (matched by entity index) or is detached when the entity is gone, and
// finally the unmatched entities, nearest the camera microphone first, are attached --
// culling the farthest attached entity for the first one that finds no free state and
// stopping after that attach.
// ---------------------------------------------------------------------------
void TrafficStateManager::UpdateParams( f32 /*lfTimeStep*/ )
{
    CgsDev::PerfMonCpu::StartMonitor( miCpuMonitor );
    CgsSound::Logic::StateManager::UpdateParams( mfTimeStepSimulation );

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
    const BrnSound::Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    CGS_ASSERT( lpInput != 0, "lpInput" );
    const BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface& lrTraffic =
        lpInput->GetTrafficOutputInterface();

    const Vector3 lListenerPosition =
        lpModule->GetEnvironment().GetMicrophoneSystem().GetMicrophone(
            CgsSound::Logic::MicrophoneSystem::E_MIC_CAMERA,
            CgsSound::Logic::MicrophoneSystem::E_PLAYER_1 )->GetMicrophoneMatrix().Pos();

    SortResult laSortedEntities[BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface::KU_MAX_ENTITIES];
    SortEntitiesDistanceFromPosition( lListenerPosition, lrTraffic, laSortedEntities );

    u16 lau16EntityMatched[KU_NUM_SLOTS] = { 0 };
    const u16 lu16EntityCount = lrTraffic.GetTrafficEntityCount();

    for ( u16 luActiveIndex = 0; luActiveIndex < KU_NUM_SLOTS; ++luActiveIndex )
    {
        Slot& lrActiveSlot = maSlots[luActiveIndex];
        if ( !lrActiveSlot.mbActive )
            continue;

        CGS_ASSERT( lrActiveSlot.mpAttachedState != 0 && lrActiveSlot.mpAttachedState->IsAttached(),
                    "( maSlots[ luActiveIndex ].mpAttachedState ) && ( maSlots[ luActiveIndex ].mpAttachedState->IsAttached() )" );

        u16 lu16Matches = 0;
        for ( u16 luEntry = 0; luEntry < lu16EntityCount; ++luEntry )
        {
            // TrafficSoundOutputInterface::GetTrafficEntity, inlined.
            const u16 lu16Index = laSortedEntities[luEntry].muIndex;
            CGS_ASSERT( lu16Index < lrTraffic.mu16EntityCount, "lu16Index < mu16EntityCount" );
            const Slot::TrafficSoundEntity& lrEntity = lrTraffic.maActiveEntityList[lu16Index];

            if ( lrEntity.mu16EntityIndex == lrActiveSlot.mEntity.mu16EntityIndex )
            {
                lrActiveSlot.mEntity = lrEntity;
                ++lau16EntityMatched[luEntry];
                ++lu16Matches;
            }
        }

        CGS_ASSERT( lu16Matches <= 1, "More than one item in list with same ID!" );
        if ( lu16Matches == 0 )
        {
            DetachEntity( lrActiveSlot );
        }
    }

    for ( u16 luEntry = 0; luEntry < lu16EntityCount; ++luEntry )
    {
        const u16 lu16Index = laSortedEntities[luEntry].muIndex;
        CGS_ASSERT( lu16Index < lrTraffic.mu16EntityCount, "lu16Index < mu16EntityCount" );
        const Slot::TrafficSoundEntity& lrEntity = lrTraffic.maActiveEntityList[lu16Index];

        if ( lau16EntityMatched[luEntry] != 0 )
            continue;

        const f32 lfHeight = std::fabs( lListenerPosition.y - lrEntity.mLocalTransform.Pos().y );
        if ( !( K_TRAFFIC_IGNORE_HEIGHT > lfHeight ) )
            continue;

        if ( AttachEntity( lrEntity ) )
            continue;

        if ( !CullIfFurtherThan( laSortedEntities[luEntry].mfDistance, lListenerPosition ) )
            break;

        // [FLAG PC witness] BRN_TRAFFICSND_DIAG
        if ( TrafficSoundDiagTake( guCullWitnesses, 16 ) )
        {
            *CgsDev::Log::gpDebugPrint
                << "[trafficsnd] cull for entity=" << static_cast<s32>( lrEntity.mu16EntityIndex )
                << " d2=" << laSortedEntities[luEntry].mfDistance.x << "\n";
        }

        if ( AttachEntity( lrEntity ) )
            break;
    }

    CgsDev::PerfMonCpu::StopMonitor( miCpuMonitor );
}

// The first inactive slot takes a copy of the entity and a free state; a slot whose
// state request comes back empty stays inactive and the next slot is tried.
bool TrafficStateManager::AttachEntity( Slot::TrafficSoundEntity lEntity )
{
    for ( u16 luSlot = 0; luSlot < KU_NUM_SLOTS; ++luSlot )
    {
        Slot& lrSlot = maSlots[luSlot];
        if ( lrSlot.mbActive )
            continue;

        lrSlot.mEntity = lEntity;
        lrSlot.mpAttachedState = GetFreeState( &lrSlot.mEntity );
        if ( lrSlot.mpAttachedState )
        {
            lrSlot.mpAttachedState->Attach( &lrSlot.mEntity );
            lrSlot.mbActive = true;

            // [FLAG PC witness] BRN_TRAFFICSND_DIAG
            if ( TrafficSoundDiagTake( guAttachWitnesses, 24 ) )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[trafficsnd] attach slot=" << static_cast<s32>( luSlot )
                    << " entity=" << static_cast<s32>( lEntity.mu16EntityIndex )
                    << " class=" << static_cast<s32>( lEntity.muVehicleClass )
                    << " engine=" << static_cast<s32>( lEntity.mbIsEngineOn )
                    << " state=" << lrSlot.mpAttachedState->GetInstanceID() << "\n";
            }
            return true;
        }
    }
    return false;
}

bool TrafficStateManager::DetachEntity( Slot& lrActiveSlot )
{
    CGS_ASSERT( lrActiveSlot.mbActive, "lActiveSlot.mbActive" );
    if ( lrActiveSlot.mpAttachedState )
    {
        const bool lbDetached = lrActiveSlot.mpAttachedState->Detach();

        // [FLAG PC witness] BRN_TRAFFICSND_DIAG
        if ( TrafficSoundDiagTake( guDetachWitnesses, 64 ) )
        {
            *CgsDev::Log::gpDebugPrint
                << "[trafficsnd] detach slot=" << static_cast<s32>( &lrActiveSlot - maSlots )
                << " entity=" << static_cast<s32>( lrActiveSlot.mEntity.mu16EntityIndex )
                << " state=" << lrActiveSlot.mpAttachedState->GetInstanceID()
                << " ok=" << ( lbDetached ? 1 : 0 ) << "\n";
        }

        if ( !lbDetached )
            return false;
        lrActiveSlot.mpAttachedState = 0;
    }
    lrActiveSlot.mbActive = false;
    return true;
}

// Releases the farthest attached entity that is farther from lPosition than the
// squared distance lfDistance; true when one was released.
bool TrafficStateManager::CullIfFurtherThan( VecFloat lfDistance, Vector3 lPosition )
{
    SortResult laCandidates[KU_NUM_SLOTS];
    s16 li16CandidateCount = 0;

    for ( u16 luSlot = 0; luSlot < KU_NUM_SLOTS; ++luSlot )
    {
        const Slot& lrSlot = maSlots[luSlot];
        if ( !lrSlot.mbActive )
            continue;

        const VecFloat lvSlotDistance =
            SplatDistance( rw::math::vpu::MagnitudeSquared( lPosition - lrSlot.mEntity.mLocalTransform.Pos() ) );
        if ( lvSlotDistance.x > lfDistance.x && lvSlotDistance.y > lfDistance.y
             && lvSlotDistance.z > lfDistance.z && lvSlotDistance.w > lfDistance.w )
        {
            laCandidates[li16CandidateCount].mfDistance = lvSlotDistance;
            laCandidates[li16CandidateCount].muIndex = luSlot;
            ++li16CandidateCount;
        }
    }

    std::sort( laCandidates, laCandidates + li16CandidateCount, SortResult::LessThanDistance );

    for ( s16 li16Candidate = static_cast<s16>( li16CandidateCount - 1 ); li16Candidate >= 0; --li16Candidate )
    {
        if ( DetachEntity( maSlots[laCandidates[li16Candidate].muIndex] ) )
            return true;
    }
    return false;
}

void TrafficStateManager::SortEntitiesDistanceFromPosition(
    Vector3 lPosition,
    const BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface& lrTrafficInterface,
    SortResult* lpResults ) const
{
    for ( u16 luIndex = 0; luIndex < lrTrafficInterface.GetTrafficEntityCount(); ++luIndex )
    {
        lpResults[luIndex].muIndex = luIndex;
        lpResults[luIndex].mfDistance = SplatDistance( rw::math::vpu::MagnitudeSquared(
            lPosition - lrTrafficInterface.maActiveEntityList[luIndex].mLocalTransform.Pos() ) );
    }

    std::sort( lpResults, lpResults + lrTrafficInterface.GetTrafficEntityCount(),
               SortResult::LessThanDistance );
}

bool TrafficStateManager::SortResult::LessThanDistance( const SortResult& lrA, const SortResult& lrB )
{
    return lrB.mfDistance.x > lrA.mfDistance.x && lrB.mfDistance.y > lrA.mfDistance.y
        && lrB.mfDistance.z > lrA.mfDistance.z && lrB.mfDistance.w > lrA.mfDistance.w;
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
