#include "GameSource/Sound/Vehicles/BrnAIVehicleStateManager.h"
#include "GameSource/Sound/Vehicles/BrnVehicleState.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/AttribSys/Generated/classes/vehicleengine.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysCollectionKey.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Sound/Engines/BrnSoundLoopModelData.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"
#include "GameSource/Sound/Vehicles/BrnAISoundDiag.h"   // [DIAG] NOT IN THE X360 BINARY

#include <cstdio>
#include <cstring>

// =============================================================================
// BrnSound::Vehicles::AIVehicleStateManager -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
//
// Sources:
//   AIVehicleStateManager::CreateObject             @ 0x82702358
//   AIVehicleStateManager::Prepare                  @ 0x826EFC18
//   AIVehicleStateManager::PrepareAIEngineLoading   @ 0x826E2708
//   AIVehicleStateManager::ResourcesAreReady        @ 0x82684038
//   AIVehicleStateManager::UpdateParams             @ 0x826CA578
//   AIVehicleStateManager::UpdateVehicleLoading     @ 0x826B1C70
//   AIVehicleStateManager::GetFreeState             @ 0x826B1B28
//   AIVehicleStateManager::GetLoopModelContent      @ 0x826987A8
//   AIVehicleStateManager::GetDecelGinsuContent     @ 0x82698910
//   AIVehicleStateManager::GetTypeName              @ 0x82684028
//   AIVehicleStateManager::~AIVehicleStateManager   @ 0x82700FE0
//   ctor                                            @ 0x82700EB8 (export-set hole)
//   sTypeInfo registration                          @ 0x82C61D98 (CRT init bank)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

namespace
{

// X360 rodata off_82F2CBDC[5] (read through tools/re/x360rd.py).
static const char* const KAPC_AI_ENGINE_NAMES[AIVehicleStateManager::KI_NUMBER_OF_AUDIO_AI_ENGINES] =
{
    "AIROD_EX",
    "AI_CIVIC_EX",
    "AI_GT_ENG",
    "AI_MUST_EX",
    "AI_F1_EX",
};

// X360 rodata dword_820AA4B4[5] -- the vehicleengine attribute-collection ids the
// console renders in decimal (rw::core::stdc::ConvertI64ToA(id, buf, 10)) and
// hashes with Attrib::StringToKey.
static const s32 KAI_AI_ENGINE_ATTRIB_IDS[AIVehicleStateManager::KI_NUMBER_OF_AUDIO_AI_ENGINES] =
{
    563494,
    613976,
    565137,
    576566,
    564456,
};

// The SQUARED listener radius inside which an AI car gets an engine state:
// unk_830085D0, a .bss VecFloat the CRT thunk @0x82C61D88 fills from flt_8201C214
// == 32400.0 (180 m). Both UpdateParams @0x826CA694 and GetFreeState @0x826B1B54
// compare `vmsum3fp128(car - listener)` against it.
static const f32 KF_AI_VEHICLE_RANGE_SQUARED = 32400.0f;

// vehicleengine's AttribSys class id, as PhysicsControl::Attach @0x826CB540 passes
// it to Attrib::FindCollectionWithDefault.
static const u64 KU64_VEHICLE_ENGINE_CLASS_ID = 0x7F161D94482CB3BFull;

// The listener the AI domain measures against: the PLAYER microphone of player 1
// (module + 0x2AF0 == Environment::mMicrophoneSystem.maMicrophones[E_MIC_PLAYER]
// [E_PLAYER_1]; its current matrix wAxis sits at module + 0x2B20, the address
// UpdateParams / GetFreeState load with `lvx128 vX, module, 0x2B20`).
static const rw::math::vpu::Vector3& AIListenerPosition( CgsSound::Logic::Module* apModule )
{
    CgsSound::Logic::MicrophoneSystem::Microphone* lpMicrophone =
        static_cast<BrnSound::Module::SoundLogicModule*>( apModule )
            ->GetEnvironment().GetMicrophoneSystem().GetMicrophone(
                CgsSound::Logic::MicrophoneSystem::E_MIC_PLAYER,
                CgsSound::Logic::MicrophoneSystem::E_PLAYER_1 );
    return lpMicrophone->GetMicrophoneMatrix().Pos();
}

// vmsum3fp128 of (a - b) -- the three-lane squared distance.
static f32 DistanceSquared3( const rw::math::vpu::Vector3& arA, const rw::math::vpu::Vector3& arB )
{
    const f32 lfX = arA.x - arB.x;
    const f32 lfY = arA.y - arB.y;
    const f32 lfZ = arA.z - arB.z;
    return lfX * lfX + lfY * lfY + lfZ * lfZ;
}

// "Engines\\%08x.bundle" of CgsResource::ID::HashString(name) -- the bundle every
// loader stage in PrepareAIEngineLoading spells with CgsCore::SPrintf.
static void AIEngineBundleName( char* apcBuffer, u32 auSize, const char* apcEngineName )
{
    std::snprintf( apcBuffer, auSize, "Engines\\%08x.bundle",
                   static_cast<u32>( CgsResource::ID::HashString(
                       reinterpret_cast<const u8*>( apcEngineName ) ) ) );
}

} // namespace

const char* AIVehicleStateManager::GetAIEngineName( s32 liAIEngineIndex )
{
    CGS_ASSERT( liAIEngineIndex >= 0 && liAIEngineIndex < KI_NUMBER_OF_AUDIO_AI_ENGINES,
                "liAIEngineIndex >= 0 && liAIEngineIndex < KI_NUMBER_OF_AUDIO_AI_ENGINES" );
    return KAPC_AI_ENGINE_NAMES[liAIEngineIndex];
}

// rw::core::stdc::ConvertI64ToA(dword_820AA4B4[i], buf, 10) then Attrib::StringToKey.
u64 AIVehicleStateManager::GetAIEngineAttribKey( s32 liAIEngineIndex )
{
    CGS_ASSERT( liAIEngineIndex >= 0 && liAIEngineIndex < KI_NUMBER_OF_AUDIO_AI_ENGINES,
                "liAIEngineIndex >= 0 && liAIEngineIndex < KI_NUMBER_OF_AUDIO_AI_ENGINES" );
    char lacDecimal[32];
    std::snprintf( lacDecimal, sizeof( lacDecimal ), "%d", KAI_AI_ENGINE_ATTRIB_IDS[liAIEngineIndex] );
    return static_cast<u64>( Attrib::StringToKey( lacDecimal ) );
}

// ---------------------------------------------------------------------------
// ctor @ 0x82700EB8 (export-set hole). CreateObject @0x82702358 `bl`s it into an
// 880-byte block; the dtor @0x82700FE0 unwinds exactly the base content pool and
// the 60 Content sub-objects (5x10 loop, 5 accel, 5 decel), so the ctor is the
// VehicleStateManager base construction + the 60 Content default constructions +
// the loading state at BEGIN.
// ---------------------------------------------------------------------------
AIVehicleStateManager::AIVehicleStateManager()
    : BrnSound::Vehicles::VehicleStateManager()
    , meAIEngineLoadingState( E_AI_ENGINE_LOADING_BEGIN )
{
}

// ---------------------------------------------------------------------------
// ~AIVehicleStateManager @ 0x82700FE0: releases maGinsuDecelContentSpecs[4..0],
// maGinsuAccelContentSpecs[4..0], maLoopContentSpecs[49..0] (each `&off_820B3250 ;
// Release if refcount hits 0`), then the base RegisteredContent pool. The host
// compiler emits the same reverse member destruction from the Content destructor.
// ---------------------------------------------------------------------------
AIVehicleStateManager::~AIVehicleStateManager()
{
}

// CreateObject @ 0x82702358: MemBase::operator new(880, "AIVehicleStateManager",
// flavour) + ctor; the int argument only picks the operator-new flavour.
CgsSound::Logic::StateManager* AIVehicleStateManager::CreateObject( u32 /*luType*/ )
{
    return new AIVehicleStateManager();
}

// Descriptor 0x82F2E88C: {ObjectID 2, "AIVehicleStateManager", base
// StateManager::sTypeInfo (0x82F2FAA0), &CreateObject @0x82702358}.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* AIVehicleStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        2,
        "AIVehicleStateManager",
        CgsSound::Logic::StateManager::GetStaticTypeInfo(),
        &AIVehicleStateManager::CreateObject );
    return &sTypeInfo;
}

// CRT init bank @0x82C61D98: `addi r3, r11, 82F2E88C ; b StateManager::AddToClassTypeInfoArray`.
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpAIVehicleStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            AIVehicleStateManager::GetStaticTypeInfo() );

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* AIVehicleStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// @ 0x82684028: `lwz r3, off_82F2E890` -> the descriptor's name.
const char* AIVehicleStateManager::GetTypeName() const
{
    return "AIVehicleStateManager";
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::Prepare()  @ 0x826EFC18   (vtable +0x0C)
//
//   switch (mePrepareState) {
//     case 0: case 5: mePrepareState = 0;
//             miCpuMonitor = PerfMonCpu::AddMonitor("AI Cars", 14, 0, 1.0, r7, 1);
//     case 1: mePrepareState = 1; if (!PrepareAIEngineLoading()) return 0;
//     case 2: mePrepareState = 2;
//     case 3: mePrepareState = 3; if (!StateManager::PrepareStates(this, 16920, 3, 0)) return 0;
//     case 4: mePrepareState = 4; return 1;
//     default: return 0; }
// ---------------------------------------------------------------------------
bool AIVehicleStateManager::Prepare()
{
    switch ( mePrepareState )
    {
    case E_PREPARE_NONE:
    case E_PREPARE_RELEASED:
        mePrepareState = E_PREPARE_NONE;
        miCpuMonitor = CgsDev::PerfMonCpu::AddMonitor( "AI Cars", 14, 0, 1.0, 0, 1 );
        // fall through
    case E_PREPARE_BEGIN:
        mePrepareState = E_PREPARE_BEGIN;
        if ( !PrepareAIEngineLoading() )
            return false;
        // fall through
    case E_PREPARE_UPDATING:
        mePrepareState = E_PREPARE_UPDATING;
        // fall through
    case E_PREPARE_STATES:
        mePrepareState = E_PREPARE_STATES;
        if ( !PrepareStates( KI_AI_STATE_EFFECT_MASK, KI_NUMBER_OF_AUDIO_AI_CAR_STATES, 0 ) )
            return false;
        // fall through
    case E_PREPARE_FINISHED:
        mePrepareState = E_PREPARE_FINISHED;
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::PrepareAIEngineLoading()  @ 0x826E2708  (DWARF cpp:167)
//
// The seven-state loader over meAIEngineLoadingState (this+0x98):
//   0 BEGIN: for each of the 5 engines: assert strlen < 64 ("String too long",
//     CgsStringUtils.h:55); bundle = "Engines\%08x.bundle" of HashString(name);
//     LoadAsset(bundle, name, E_ATTRIBSYS); LoadAsset(bundle, "%sRegistry", E_DATA).
//   1 WAITING_FOR_LOAD_ATTRIB: return false (ResourcesAreReady moves 1 -> 2).
//   2 BEGIN_LOADING_ENGINE_COMPONENTS: for each: vehicleengine attribs of
//     StringToKey(decimal id); assert LoopModel() non-null and non-empty ("Bad
//     Engine Data", cpp:231/232); LoadAsset(bundle, LoopModel(), E_DATA).
//   3 WAITING_LOADING_ENGINE_COMPONENTS: return false (ResourcesAreReady 3 -> 4).
//   4 CREATING_CONTENT_SPECS: for each: AddRegistry(name, true); GetAsset(bundle,
//     LoopModel()) -> LoopModelData; assert muNumOfPartials <= KI_MAX_LOOPS
//     (cpp:272); maLoopContentSpecs[i][j].Construct(module, factory,
//     partial[j].mWaveName.mHash); maGinsuAccelContentSpecs[i] <- MakeHash(
//     GinsuFileAccel()); maGinsuDecelContentSpecs[i] <- MakeHash(GinsuFileDecel()).
//   5 WAITING_FOR_CONTENT_SPECS: every created loop spec, the accel and the decel
//     spec of every engine must report loaded ((content+30 & 0x7F) == 3), else
//     return false; then state = 6, return true.
//   6 FINISHED: return true.   default: return false.
// ---------------------------------------------------------------------------
bool AIVehicleStateManager::PrepareAIEngineLoading()
{
    CgsSound::Logic::Module* lpModule = GetLogicModule();
    char lacBundle[64];
    char lacRegistry[64];

    switch ( meAIEngineLoadingState )
    {
    case E_AI_ENGINE_LOADING_BEGIN:
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_BEGIN;
        for ( s32 liEngine = 0; liEngine < KI_NUMBER_OF_AUDIO_AI_ENGINES; ++liEngine )
        {
            const char* lpcName = KAPC_AI_ENGINE_NAMES[liEngine];
            CGS_ASSERT( std::strlen( lpcName ) < 64, "String too long" );
            AIEngineBundleName( lacBundle, sizeof( lacBundle ), lpcName );
            LoadAsset( lacBundle, lpcName, BrnSound::Logic::ResourceRegistrar::E_ATTRIBSYS );
            std::snprintf( lacRegistry, sizeof( lacRegistry ), "%sRegistry", lpcName );
            LoadAsset( lacBundle, lacRegistry, BrnSound::Logic::ResourceRegistrar::E_DATA );

            // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG
            if ( AISoundDiagLive() )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-sound-load] request engine=" << lpcName
                    << " bundle=" << lacBundle
                    << " registry=" << lacRegistry
                    << "\n";
            }
        }
        // fall through
    case E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB:
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB;
        return false;

    case E_AI_ENGINE_LOADING_BEGIN_LOADING_ENGINE_COMPONENTS:
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_BEGIN_LOADING_ENGINE_COMPONENTS;
        for ( s32 liEngine = 0; liEngine < KI_NUMBER_OF_AUDIO_AI_ENGINES; ++liEngine )
        {
            Attrib::Gen::vehicleengine lAttribs(
                Attrib::FindCollectionWithDefault( KU64_VEHICLE_ENGINE_CLASS_ID,
                                                   GetAIEngineAttribKey( liEngine ) ),
                0 );
            AIEngineBundleName( lacBundle, sizeof( lacBundle ), KAPC_AI_ENGINE_NAMES[liEngine] );
            const char* lpcLoopModel = lAttribs.LoopModel();
            CGS_ASSERT( lpcLoopModel != 0, "Bad Engine Data" );
            CGS_ASSERT( lpcLoopModel == 0 || std::strcmp( lpcLoopModel, "" ) != 0, "Bad Engine Data" );
            LoadAsset( lacBundle, lpcLoopModel, BrnSound::Logic::ResourceRegistrar::E_DATA );

            // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG
            if ( AISoundDiagLive() )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-sound-load] request loopmodel engine=" << KAPC_AI_ENGINE_NAMES[liEngine]
                    << " resource=" << ( lpcLoopModel ? lpcLoopModel : "<null>" )
                    << "\n";
            }
        }
        // fall through
    case E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS:
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS;
        return false;

    case E_AI_ENGINE_LOADING_CREATING_CONTENT_SPECS:
    {
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_CREATING_CONTENT_SPECS;
        const u32 luFactoryName = static_cast<u32>(
            CgsSound::Playback::GenericRwacFactorySkName().GetValue() );   // dword_83008650
        for ( s32 liEngine = 0; liEngine < KI_NUMBER_OF_AUDIO_AI_ENGINES; ++liEngine )
        {
            Attrib::Gen::vehicleengine lAttribs(
                Attrib::FindCollectionWithDefault( KU64_VEHICLE_ENGINE_CLASS_ID,
                                                   GetAIEngineAttribKey( liEngine ) ),
                0 );
            AddRegistry( KAPC_AI_ENGINE_NAMES[liEngine], true );
            AIEngineBundleName( lacBundle, sizeof( lacBundle ), KAPC_AI_ENGINE_NAMES[liEngine] );

            CgsResource::ResourceHandle lHandle = GetAsset( lacBundle, lAttribs.LoopModel() );
            CgsResource::ResourcePtr<BrnSound::Vehicles::Engines::LoopModelData> lLoopModel( lHandle );
            const BrnSound::Vehicles::Engines::LoopModelData* lpLoopModel = lLoopModel.GetMemoryResource();
            const u32 luNumberOfLoops = lpLoopModel ? lpLoopModel->muNumOfPartials : 0;
            CGS_ASSERT( luNumberOfLoops <= static_cast<u32>( KI_MAX_LOOPS ),
                        "liNumberOfLoops <= BrnSound::Vehicles::Engines::DualGinsuEffect::KI_MAX_LOOPS" );
            for ( u32 luLoop = 0; luLoop < luNumberOfLoops && luLoop < static_cast<u32>( KI_MAX_LOOPS ); ++luLoop )
            {
                maLoopContentSpecs[liEngine][luLoop].Construct(
                    lpModule, luFactoryName, lpLoopModel->mpaPartials[luLoop].mWaveName.mHash );
            }
            maGinsuAccelContentSpecs[liEngine].Construct(
                lpModule, luFactoryName,
                static_cast<u32>( CgsSound::Playback::Name::MakeHash( lAttribs.GinsuFileAccel() ) ) );
            maGinsuDecelContentSpecs[liEngine].Construct(
                lpModule, luFactoryName,
                static_cast<u32>( CgsSound::Playback::Name::MakeHash( lAttribs.GinsuFileDecel() ) ) );

            // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG
            if ( AISoundDiagLive() )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-sound-load] specs engine=" << KAPC_AI_ENGINE_NAMES[liEngine]
                    << " loopmodel=" << ( lpLoopModel ? "resolved" : "MISSING" )
                    << " loops=" << static_cast<s32>( luNumberOfLoops )
                    << " accel=" << ( lAttribs.GinsuFileAccel() ? lAttribs.GinsuFileAccel() : "<null>" )
                    << " decel=" << ( lAttribs.GinsuFileDecel() ? lAttribs.GinsuFileDecel() : "<null>" )
                    << "\n";
            }
        }
        // fall through
    }
    case E_AI_ENGINE_LOADING_WAITING_FOR_CONTENT_SPECS:
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_WAITING_FOR_CONTENT_SPECS;
        for ( s32 liEngine = 0; liEngine < KI_NUMBER_OF_AUDIO_AI_ENGINES; ++liEngine )
        {
            for ( s32 liLoop = 0; liLoop < KI_MAX_LOOPS && maLoopContentSpecs[liEngine][liLoop].IsCreated(); ++liLoop )
            {
                if ( !maLoopContentSpecs[liEngine][liLoop].IsLoaded() )
                    return false;
            }
            if ( !( maGinsuAccelContentSpecs[liEngine].IsCreated() && maGinsuAccelContentSpecs[liEngine].IsLoaded() ) )
                return false;
            if ( !( maGinsuDecelContentSpecs[liEngine].IsCreated() && maGinsuDecelContentSpecs[liEngine].IsLoaded() ) )
                return false;
        }
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_FINISHED;

        // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG
        if ( AISoundDiagLive() )
            *CgsDev::Log::gpDebugPrint << "[ai-sound-load] all AI engine content specs loaded\n";
        return true;

    case E_AI_ENGINE_LOADING_FINISHED:
        return true;

    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::ResourcesAreReady()  @ 0x82684038
//   assert(state == WAITING_FOR_LOAD_ATTRIB || state == WAITING_LOADING_ENGINE_COMPONENTS)
//   1 -> 2 ; 3 -> 4   (BrnAIVehicleStateManager.cpp:354)
// ---------------------------------------------------------------------------
void AIVehicleStateManager::ResourcesAreReady()
{
    CGS_ASSERT( meAIEngineLoadingState == E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB
                || meAIEngineLoadingState == E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS,
                "E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB == meAIEngineLoadingState || "
                "E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS == meAIEngineLoadingState" );

    if ( meAIEngineLoadingState == E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB )
    {
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_BEGIN_LOADING_ENGINE_COMPONENTS;
    }
    else if ( meAIEngineLoadingState == E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS )
    {
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_CREATING_CONTENT_SPECS;
    }

    // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG
    if ( AISoundDiagLive() )
    {
        *CgsDev::Log::gpDebugPrint
            << "[ai-sound-load] resources ready -> loading state " << static_cast<s32>( meAIEngineLoadingState )
            << "\n";
    }
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::UpdateParams(f32)  @ 0x826CA578  (vtable +0x18)
//
//   if (mePrepareState != 4) return;
//   PerfMonCpu::StartMonitor(miCpuMonitor);
//   StateManager::UpdateParams(this, mfTimeStepSimulation);      ; lfs f1, 0x10(this)
//   input = module->mpBrnLogicInputBuffer (assert); vehicles = input->GetVehicleInterface() (assert "lpInput")
//   player = vehicles->IsPlayerCarActive() ? vehicles->GetPlayerActiveRaceCarIndex() : -1;
//   UpdateVehicleLoading(player);
//   if (IsDataLoaded()) {                                        ; vtable +0x24
//     for (i = 0; i < 8; ++i)
//       if (i != player && vehicles->IsRaceCarActive(i) && gaDesiredAssetIds[i].lo != 0) {
//         rc = vehicles->GetRaceCarState(i);
//         if (state = GetStateObj(rc)) { if (|rc.pos - listener|^2 > 32400) state->Detach(); }
//         else if (state = GetFreeState(rc)) {
//           AttachInfo::Construct(gaDesiredAssetIds[i], gapLoadedVehicleEntries[i], i);
//           state->Attach(&info); StopMonitor; return; }              ; ONE attach per frame
//     dword_82F2CBD8 = -1; <dev-only nearest-state DebugRender::DrawSphere, gated on
//     byte_82FFB812 || byte_82FFB813 -- not carried, it draws nothing the PC build has>
//   }
//   PerfMonCpu::StopMonitor(miCpuMonitor);
// ---------------------------------------------------------------------------
void AIVehicleStateManager::UpdateParams( f32 /*af32DeltaTime*/ )
{
    if ( mePrepareState != E_PREPARE_FINISHED )
        return;

    CgsDev::PerfMonCpu::StartMonitor( miCpuMonitor );
    CgsSound::Logic::StateManager::UpdateParams( mfTimeStepSimulation );

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
    BrnSound::Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    CGS_ASSERT( lpInput != 0, "mpBrnLogicInputBuffer" );
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicles =
        lpInput->GetVehicleInterface();
    CGS_ASSERT( lpVehicles != 0, "lpInput" );

    const s32 liPlayer = lpVehicles->IsPlayerCarActive()
        ? static_cast<s32>( lpVehicles->GetPlayerActiveRaceCarIndex() ) : -1;
    UpdateVehicleLoading( static_cast<EActiveRaceCarIndex>( liPlayer ) );

    if ( IsDataLoaded() )
    {
        const rw::math::vpu::Vector3& lrListener = AIListenerPosition( lpModule );
        for ( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
        {
            const EActiveRaceCarIndex leCar = static_cast<EActiveRaceCarIndex>( liCar );
            if ( liCar == liPlayer
                 || !lpVehicles->IsRaceCarActive( leCar )
                 || GetLoadedAssetId( static_cast<u32>( liCar ) ) == 0 )
            {
                continue;
            }

            const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState = lpVehicles->GetRaceCarState( leCar );

            // =============== [FLAG PC bring-up] DO NOT ATTACH TO A CAR THAT DOES NOT EXIST YET.
            // The console has no test here, because its own flow makes one unnecessary:
            // VehicleManager::ProcessCreateEvents creates the entity-module slot and the physics
            // slot TOGETHER, so a car that reports IsRaceCarActive always has a populated
            // RaceCarState. On this build it does not -- the same split the race-car readback's
            // mUsedRaceCars gate already documents (BrnRaceCarEntityModule.cpp: "nothing
            // populates VehicleOutputInterface::maRaceCarStates yet").
            //
            // MEASURED, owner-flow run 2026-09-16 (scratch/flow_run/carT), by log line number:
            //   7264  [ai-sound-attach] car=1 live entity=0x0  <- we attach here
            //   7521  [seat] car 1 ...                         <- the car is CREATED here
            //   7525  [racecar-id] slot 1 entityWord 0x01000400 <- and gets its identity here
            // i.e. the attach ran 257 log lines BEFORE the car existed, over an all-zero slot.
            //
            // WHY THAT ZERO IS EXPENSIVE. VehicleState::Attach copies the snapshot ONCE
            // (mVehiclePhysicsData = *lpRaceCarState) and VehicleState::UpdateParams only
            // refreshes it after meUpdateState reaches E_UPDATE_ATTACHED -- so the state keeps
            // the zero across the whole attach/load window. VehicleState::IsAttachedToThis
            // @0x82683D38 is an IDENTITY TEST on exactly that field, so every zero-id state
            // matches every zero-id car: in the owner's session car 1's out-of-range test
            // resolved to car 2's state and detached it, car 2 re-attached next frame, and the
            // two alternated 236 times (strictly interleaved in the log). Each re-attach
            // re-requested InAir.bundle + the engine bundle without releasing, filling the
            // 16-node requester pool -- "We've run out of nodes." x468.
            //
            // THE TEST IS THE CONSOLE'S OWN, on the same struct. VehicleState::UpdateParams
            // (BrnVehicleState.cpp) already spells "this RaceCarState is real" as
            //     IsRaceCarActive(idx) && lpState != 0 && lpState->mCarAssetAttribKey != 0
            // and that is reproduced from the X360 body. Applied here it is the same predicate
            // at the one place that needs it, in the shape this loop already uses for
            // "not ready yet" (the GetLoadedAssetId(liCar) == 0 continue above).
            // DELETE-WHEN ProcessCreateEvents claims the physics slot at entity-module
            // activation, making IsRaceCarActive sufficient again as it is on the console.
            if ( lpRaceCarState == 0 || lpRaceCarState->mCarAssetAttribKey == 0 )
            {
                continue;
            }

            CgsSound::Logic::State* lpState = GetStateObj( const_cast<BrnPhysics::Vehicle::RaceCarState*>( lpRaceCarState ) );

            // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG. The live entity id
            // GetStateObj keys on (VehicleState::IsAttachedToThis @0x82683D38), once per
            // car: an id of 0 on every car makes every attached state match every car.
            if ( AISoundDiagLive() )
            {
                static u8 sau8Seen[E_ACTIVE_RACE_CAR_INDEX_COUNT] = { 0 };
                if ( !sau8Seen[liCar] )
                {
                    sau8Seen[liCar] = 1;
                    *CgsDev::Log::gpDebugPrint
                        << "[ai-sound-attach] car=" << liCar
                        << " live entity=" << CgsDev::E_PRINTMODE_HEXONCE
                        << static_cast<u64>( lpRaceCarState->mEntityId.muValue )
                        << " d2=" << DistanceSquared3( lpRaceCarState->mTransform.Pos(), lrListener )
                        << " state=" << ( lpState ? lpState->GetInstanceID() : -1 )
                        << "\n";
                }
            }
            if ( lpState )
            {
                if ( DistanceSquared3( lpRaceCarState->mTransform.Pos(), lrListener ) > KF_AI_VEHICLE_RANGE_SQUARED )
                {
                    // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG
                    if ( AISoundDiagLive() )
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[ai-sound-detach] car=" << liCar << " out of range (d2="
                            << DistanceSquared3( lpRaceCarState->mTransform.Pos(), lrListener ) << ")\n";
                    }
                    lpState->Detach();
                }
            }
            else
            {
                lpState = GetFreeState( const_cast<BrnPhysics::Vehicle::RaceCarState*>( lpRaceCarState ) );
                if ( lpState )
                {
                    VehicleState::AttachInfo lInfo;
                    lInfo.Construct( GetLoadedAssetId( static_cast<u32>( liCar ) ),
                                     const_cast<BrnResource::VehicleListEntry*>(
                                         GetLoadedVehicleEntry( static_cast<u32>( liCar ) ) ),
                                     static_cast<u32>( liCar ) );
                    lpState->Attach( &lInfo );
                    CgsDev::PerfMonCpu::StopMonitor( miCpuMonitor );
                    return;
                }
            }
        }
    }

    CgsDev::PerfMonCpu::StopMonitor( miCpuMonitor );
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::UpdateVehicleLoading(EActiveRaceCarIndex)  @ 0x826B1C70  (DWARF cpp:614)
//
// Walks the eight active-race-car slots of the module-wide VehicleStateManager
// tables (X360 globals: qword_82FFB380 desired ids, qword_82FFB3C8 attached ids,
// qword_82FFB370 added mask, qword_82FFB3C0 desired-player mask, qword_82FFB408
// attached mask, qword_82FFB378 attached-player mask -- the names Nathan V.'s
// AddEntry / OnAssetLoaded / OnAssetUnloaded reconstructions gave them) and, per
// slot, with the console's own TTY strings:
//   a) attached && !attachedPlayer && desiredPlayer
//        -> "OnAssetUnloaded becahse desired is player and loaded is not. Car idx"
//   b) attachedId != desiredId && attached
//        -> "OnAssetUnloaded because desired does not mach loaded. Car idx"
//   c) desiredId != 0 && !desiredPlayer && attachedId == 0 && !attached
//        -> "OnAssetLoaded because desired does mach loaded. Car idx"
//           OnAssetLoaded(desiredId, i, false): the AI engines are preloaded, so an
//           AI entry counts as loaded the moment it is desired, and the game gets
//           its AudioCarDataLoadedEvent straight away.
// The player index argument is not read by the console body (r4 is dead).
// ---------------------------------------------------------------------------
void AIVehicleStateManager::UpdateVehicleLoading( EActiveRaceCarIndex /*leActiveRaceCarIndex*/ )
{
    for ( u32 luCar = 0; luCar < static_cast<u32>( E_ACTIVE_RACE_CAR_INDEX_COUNT ); ++luCar )
    {
        const bool  lbAttached       = IsAssetAttached( luCar );
        const bool  lbAttachedPlayer = IsAttachedEntryPlayer( luCar );
        const bool  lbDesiredPlayer  = IsDesiredEntryPlayer( luCar );
        const CgsID lDesiredId       = GetLoadedAssetId( luCar );
        const CgsID lAttachedId      = GetAttachedAssetId( luCar );

        if ( lbAttached && !lbAttachedPlayer && lbDesiredPlayer )
        {
            OnAssetUnloaded( lAttachedId, luCar );
        }

        if ( GetAttachedAssetId( luCar ) != GetLoadedAssetId( luCar ) )
        {
            if ( IsAssetAttached( luCar ) )
                OnAssetUnloaded( GetAttachedAssetId( luCar ), luCar );
        }

        if ( GetLoadedAssetId( luCar ) != 0 )
        {
            if ( !IsDesiredEntryPlayer( luCar ) && GetAttachedAssetId( luCar ) == 0 )
            {
                if ( !IsAssetAttached( luCar ) )
                    OnAssetLoaded( GetLoadedAssetId( luCar ), luCar, false );
            }
        }
        (void)lDesiredId;
    }
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::GetFreeState(void*)  @ 0x826B1B28  (vtable +0x14; DWARF cpp:540)
//
//   rc = (RaceCarState*)apv;
//   if (|rc->mTransform.pos - listener|^2 > 32400) return 0;          ; unk_830085D0
//   for (s = mpHeadState; s; s = s->next) if (!s->mbIsAttached) return s;
//   for (s = mpHeadState; s; s = s->next)
//     if (s->mbIsAttached
//         && |s->mVehiclePhysicsData.mTransform.pos - listener|^2 > |rc.pos - listener|^2
//         && s->Detach())                                             ; vtable +0x18
//       return s;                                                     ; steal the farther car's state
//   return 0;
// ---------------------------------------------------------------------------
CgsSound::Logic::State* AIVehicleStateManager::GetFreeState( void* apvAttachment )
{
    const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
        static_cast<const BrnPhysics::Vehicle::RaceCarState*>( apvAttachment );
    const rw::math::vpu::Vector3& lrListener = AIListenerPosition( GetLogicModule() );

    const f32 lfCandidateDistanceSquared = DistanceSquared3( lpRaceCarState->mTransform.Pos(), lrListener );
    if ( lfCandidateDistanceSquared > KF_AI_VEHICLE_RANGE_SQUARED )
        return 0;

    for ( CgsSound::Logic::State* lpState = GetHeadState(); lpState; lpState = lpState->GetNextState() )
    {
        if ( !lpState->IsAttached() )
            return lpState;
    }

    for ( CgsSound::Logic::State* lpState = GetHeadState(); lpState; lpState = lpState->GetNextState() )
    {
        if ( !lpState->IsAttached() )
            continue;
        const VehicleState* lpVehicleState = static_cast<const VehicleState*>( lpState );
        if ( DistanceSquared3( lpVehicleState->GetVehicleData()->mTransform.Pos(), lrListener )
             > lfCandidateDistanceSquared )
        {
            if ( lpState->Detach() )
                return lpState;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// GetLoopModelContent @ 0x826987A8 / GetDecelGinsuContent @ 0x82698910 /
// GetAccelGinsuContent (DWARF h:168, the "12*(i+63)" row of the same pattern).
// ---------------------------------------------------------------------------
const CgsSound::Logic::Content* AIVehicleStateManager::GetLoopModelContent( s32 liAIEngineIndex, u32 lLoopIndex )
{
    CGS_ASSERT( liAIEngineIndex >= 0 && liAIEngineIndex < KI_NUMBER_OF_AUDIO_AI_ENGINES,
                "liAIEngineIndex >= 0 && liAIEngineIndex < KI_NUMBER_OF_AUDIO_AI_ENGINES" );
    CGS_ASSERT( lLoopIndex < static_cast<u32>( KI_MAX_LOOPS ),
                "lLoopIndex < BrnSound::Vehicles::Engines::DualGinsuEffect::KI_MAX_LOOPS" );
    const CgsSound::Logic::Content* lpContent = &maLoopContentSpecs[liAIEngineIndex][lLoopIndex];
    CGS_ASSERT( lpContent->IsLoaded(), "maLoopContentSpecs[liAIEngineIndex][lLoopIndex].IsLoaded()" );
    return lpContent;
}

const CgsSound::Logic::Content* AIVehicleStateManager::GetAccelGinsuContent( s32 liAIEngineIndex )
{
    CGS_ASSERT( liAIEngineIndex >= 0 && liAIEngineIndex < KI_NUMBER_OF_AUDIO_AI_ENGINES,
                "liAIEngineIndex >= 0 && liAIEngineIndex < KI_NUMBER_OF_AUDIO_AI_ENGINES" );
    const CgsSound::Logic::Content* lpContent = &maGinsuAccelContentSpecs[liAIEngineIndex];
    CGS_ASSERT( lpContent->IsLoaded(), "maGinsuAccelContentSpecs[liAIEngineIndex].IsLoaded()" );
    return lpContent;
}

const CgsSound::Logic::Content* AIVehicleStateManager::GetDecelGinsuContent( s32 liAIEngineIndex )
{
    CGS_ASSERT( liAIEngineIndex >= 0 && liAIEngineIndex < KI_NUMBER_OF_AUDIO_AI_ENGINES,
                "liAIEngineIndex >= 0 && liAIEngineIndex < KI_NUMBER_OF_AUDIO_AI_ENGINES" );
    const CgsSound::Logic::Content* lpContent = &maGinsuDecelContentSpecs[liAIEngineIndex];
    CGS_ASSERT( lpContent->IsLoaded(), "maGinsuDecelContentSpecs[liAIEngineIndex].IsLoaded()" );
    return lpContent;
}

} // namespace Vehicles
} // namespace BrnSound
