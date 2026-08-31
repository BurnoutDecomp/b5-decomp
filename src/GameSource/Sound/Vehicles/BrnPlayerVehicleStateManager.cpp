#include "GameSource/Sound/Vehicles/BrnPlayerVehicleStateManager.h"
#include "GameSource/Sound/Vehicles/BrnVehicleState.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

// =============================================================================
// BrnSound::Vehicles::PlayerVehicleStateManager -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// This canonical home restores the player-car engine/vehicle sound-logic state
// manager constructed by StateManager::CreateStateMan @ 0x826A5B60, including its
// content banks, world-scene placement, state preparation, and per-frame attach path.
//
// Sources:
//   PlayerVehicleStateManager::CreateObject  @ 0x827022D8
//   PlayerVehicleStateManager::Prepare       @ 0x826EF428
//   PlayerVehicleStateManager::UpdateParams  @ 0x826EF7E8
//   PlayerVehicleStateManager::Release       @ 0x826EFBB8
//   PlayerVehicleStateManager::ctor          @ 0x82700BE8
//   PlayerVehicleStateManager::dtor          @ 0x82700D30
// DecFIGS supplies the registration names/ObjectIDs and declaration shape where
// ARTIST stripped the corresponding symbols.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::PlayerVehicleStateManager()  ctor @ 0x82700BE8
//
//   result = CgsSound::Logic::StateManager::StateManager();   ; base ctor
//   *(result+144) = off_820AB608;                             ; (transient) +0x90 vtable
//   *result       = off_820B87A0;                             ; primary vtable @ +0
//   *(result+144) = off_820B8798;                             ; IResourceRequester vtable @ +0x90
//   *(result+152) = off_820B1738;                             ; VehicleStateManager base leg
//   <build seven content sub-objects at +0x42C..+0x47C, each {&off_820B3250,0,0}>
//   <seed scalar tail at +0x41C..+0x428>
//   <THREE global-static table init loops: dword_82FFB350[8]=0; qword_82FFB3C8[8]=self;
//    qword_82FFB380[8]=self; + qword_82FFB370/408/378 = &unk_83000000 sentinel>
//   return result;
//
// The host declaration expresses the secondary base through VehicleStateManager and
// names the seven Content members explicitly. Their constructors reproduce the seven
// refcounted sub-object initialisations; normal C++ member teardown supplies the
// reverse release order in the destructor.
// ---------------------------------------------------------------------------
PlayerVehicleStateManager::PlayerVehicleStateManager()
    : BrnSound::Vehicles::VehicleStateManager()
    , mSoundScene()
    , mCsisDeformationInterface()
    , mCsisBoostInterface()
    , mCsisSkidInterface()
    , mCsisInAirInteface()
    , mCsisSurfaces()
    , mCsisTurbo()
    , mCsisGearWhine()
{
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::~PlayerVehicleStateManager()  @ 0x82700D30  (the X360 `vector deleting destructor`)
//
//   for each of the SEVEN content sub-objects (a1[285],a1[282],...,a1[267]):
//       a1[n] = &off_820B3250; if (a1[n+1]) CgsSound::Playback::Object::Release(...);  ; drop each bank
//   *a1 = off_820B66E4;
//   CgsSound::Logic::StateManager::RegisteredContent_4_int_::~ObjectPool(a1 + 12);     ; base pool teardown
//   *a1 = &off_820AA820;                                                               ; MemBase vtable
//
// The empty body is intentional: the host compiler emits reverse member destruction
// for the seven Content objects and then the VehicleStateManager/base teardown, which
// is the semantic equivalent of the explicit console deleting-destructor sequence.
// ---------------------------------------------------------------------------
PlayerVehicleStateManager::~PlayerVehicleStateManager()
{
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::CreateObject(u32)  @ 0x827022D8   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(1152, "PlayerVehicleStateManager", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(1152, "PlayerVehicleStateManager", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 1152-byte (0x480) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "PlayerVehicleStateManager" (off_82F2E880) and
// placement-constructs a PlayerVehicleStateManager into it. Both arms call the SAME
// size+ctor; the `a1` argument only selects the operator-new flavour (0/1). The
// factory CreateStateMan @ 0x826A5B60 calls this as createObject(0).
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model operator
// new(size, tag, flavour) -- the sound allocator (off_82FFB954) is not homed in this
// group -- so a faithful placement-new through that allocator is not yet expressible.
// This reconstruction uses the host `new` (global operator new, NOT the sound
// allocator); the observable result -- a constructed PlayerVehicleStateManager* (or
// null) handed to the factory -- matches. Replace with the sound-allocator placement-
// new once MemBase::operator new is homed. The 1152-byte size is the X360 0x480; on
// the 64-bit host the real object differs in size, so the literal is documentation
// only and is NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* PlayerVehicleStateManager::CreateObject( u32 /*luType*/ )
{
    return new PlayerVehicleStateManager();
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::GetStaticTypeInfo()  (RTTI descriptor)
//
// Mirrors the in-tree GetStaticTypeInfo convention (CgsStateManager.cpp:230). A
// function-local static ClassTypeInfo<StateManager> seeded with (ObjectID, typeName,
// baseTypeInfo, createObject) so the factory CreateStateMan can match
// descriptor->ObjectID and call ->createObject.
//
// DecFIGS' static initializer pins this manager's ObjectID to 1 and its base descriptor
// to StateManager. The file-scope registration below reproduces the original registry
// insertion before the sound module asks the factory to create its managers.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* PlayerVehicleStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        1,                              // ObjectID (PS3 DecFIGS static-init 0x85FA1C: PlayerVehicleStateManager=1)
        "PlayerVehicleStateManager",    // typeName
        CgsSound::Logic::StateManager::GetStaticTypeInfo(), // baseTypeInfo (PS3 0x85FA1C: =StateManager::GetStaticTypeInfo())
        &PlayerVehicleStateManager::CreateObject // createObject
    );
    return &sTypeInfo;
}

// ---------------------------------------------------------------------------
// File-scope registration (Part D): land this leaf's descriptor in the shared
// StateManager RTTI registry (CgsStateManager.cpp gapClassTypeInfoArray, X360
// dword_82FFBC58) at load time, so StateManager::CreateStateMan (0x826A5B60) can
// find it by ObjectID. AddToClassTypeInfoArray is the canonical StateManager
// registration entry (@ 0x8268DFE8), reached through the BrnStateManager base.
//
// ObjectID RESOLVED (PS3 DecFIGS static-init 0x85FA1C): PlayerVehicleStateManager::sTypeInfo
// .ObjectID = 1. The descriptor comes from GetStaticTypeInfo() (seeded with that id and
// baseTypeInfo = StateManager::GetStaticTypeInfo()), so this registration lands the real
// id. NOTE (2026-08-25): this TU IS in the game build -- the registration runs at
// static-init and CreateStateManagers constructs this manager at boot.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpPlayerVehicleStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            PlayerVehicleStateManager::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* PlayerVehicleStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::GetTypeName() const
//   The X360 leaf GetTypeName loads the tag string (off_82F2E880) used by
//   CreateObject's operator new -> returns the literal "PlayerVehicleStateManager".
// ---------------------------------------------------------------------------
const char* PlayerVehicleStateManager::GetTypeName() const
{
    return "PlayerVehicleStateManager";
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::Prepare()  @ 0x826EF428   (vtable +0x0C)
//
// X360 body: a switch on the +0x24 prepare-state (cases 0/5 -> 0, 1, 2, 3, 4):
//   state 1: Content::Construct + LoadAsset the player engine/component banks
//            (StateManager::GetContent feeds the per-component content);
//            SoundWorldScene::Prepare (the player 3D voice scene);
//   state 2: if (!Content::IsLoaded(...)) return 0;
//            mCpuMonitor = PerfMonCpu::AddMonitor("Player Car", 14, 0, 1.0, ...)
//   state 3: if (!StateManager::PrepareStates(...)) return 0;
//   state 4: return 1;
//
// The host body preserves that state machine: construct/load the component banks,
// wait for both direct and broker-registered content, create the CPU monitor, prepare
// the selected player-vehicle states, and only then report completion.
// ---------------------------------------------------------------------------
bool PlayerVehicleStateManager::Prepare()
{
    CgsSound::Logic::Module* lpModule = GetLogicModule();
    const u32 luFactoryName = static_cast<u32>(
        CgsSound::Playback::AemsFactorySkName().GetValue());

    switch (GetPrepareState())
    {
    case E_PREPARE_NONE:
    case E_PREPARE_RELEASED:
        mePrepareState = E_PREPARE_NONE;
        // fall through
    case E_PREPARE_BEGIN:
        mePrepareState = E_PREPARE_BEGIN;
        mCsisDeformationInterface.Construct(lpModule, luFactoryName,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("CrumpleCsis")));
        mCsisBoostInterface.Construct(lpModule, luFactoryName,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("BoostCsis")));
        mCsisSkidInterface.Construct(lpModule, luFactoryName,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("SkidsCsis")));
        mCsisInAirInteface.Construct(lpModule, luFactoryName,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("InAirCsis")));
        mCsisSurfaces.Construct(lpModule, luFactoryName,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("SurfaceCsis")));
        mCsisTurbo.Construct(lpModule, luFactoryName,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("TurboCsis")));
        mCsisGearWhine.Construct(lpModule, luFactoryName,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("GearWhineCsis")));
        LoadAsset("sound\\aems\\surface_patch_bank.bundle", 0,
                  BrnSound::Logic::ResourceRegistrar::E_DATA);
        LoadAsset("sound\\aems\\InAir.bundle", 0,
                  BrnSound::Logic::ResourceRegistrar::E_DATA);
        LoadAsset("sound\\aems\\Skids.bundle", 0,
                  BrnSound::Logic::ResourceRegistrar::E_DATA);
        mSoundScene.Prepare(
            static_cast<BrnSound::Module::SoundLogicModule*>(lpModule), "_Passby");
        // fall through
    case E_PREPARE_UPDATING:
    {
        mePrepareState = E_PREPARE_UPDATING;
        const CgsSound::Logic::Content* lapContent[] = {
            &mCsisBoostInterface, &mCsisSkidInterface, &mCsisInAirInteface,
            &mCsisSurfaces, &mCsisTurbo, &mCsisGearWhine,
            &mCsisDeformationInterface
        };
        for (u32 luContent = 0; luContent < sizeof(lapContent) / sizeof(lapContent[0]); ++luContent)
        {
            if (!lapContent[luContent]->IsCreated() || !lapContent[luContent]->IsLoaded())
                return false;
        }

        const char* lapRegisteredNames[] = {
            "surface_patch_bank.abi", "inair.abi", "Skids.abi"
        };
        for (u32 luName = 0; luName < sizeof(lapRegisteredNames) / sizeof(lapRegisteredNames[0]); ++luName)
        {
            CgsSound::Playback::Name lName(lapRegisteredNames[luName]);
            CgsSound::Logic::Content* lpContent = GetContent(lName);
            if (!lpContent || !lpContent->IsCreated() || !lpContent->IsLoaded())
                return false;
        }
        miCpuMonitor = CgsDev::PerfMonCpu::AddMonitor("Player Car", 14, 0, 1.0, 0, 1);
        // fall through
    }
    case E_PREPARE_STATES:
        mePrepareState = E_PREPARE_STATES;
        if (!PrepareStates(0x1BFF9, 1, 0x4481))
            return false;
        // fall through
    case E_PREPARE_FINISHED:
        mePrepareState = E_PREPARE_FINISHED;
        return true;
    default:
        return false;
    }
}

bool PlayerVehicleStateManager::Release()
{
    mSoundScene.Release();
    mePrepareState = E_PREPARE_RELEASED;
    return true;
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::ResourcesAreReady()  (IResourceRequester completion callback)
//
// The resource broker invokes this after the three AEMS bundles resolve. Each asset is
// registered under its ABI content name and constructed through the original AEMS
// factory, making the readiness checks in Prepare's updating state meaningful.
// ---------------------------------------------------------------------------
void PlayerVehicleStateManager::ResourcesAreReady()
{
    CgsSound::Logic::Module* lpModule = GetLogicModule();
    const u32 luFactoryName = static_cast<u32>(
        CgsSound::Playback::AemsFactorySkName().GetValue());
    const char* lapNames[] = {
        "surface_patch_bank.abi", "inair.abi", "Skids.abi"
    };
    for (u32 luName = 0; luName < sizeof(lapNames) / sizeof(lapNames[0]); ++luName)
    {
        CgsSound::Playback::Name lName(lapNames[luName]);
        CgsSound::Logic::Content* lpContent = RegisterContent(lName);
        lpContent->Construct(lpModule, luFactoryName, lName.GetValue());
    }
}

void PlayerVehicleStateManager::UpdateParams(f32 af32DeltaTime)
{
    CgsDev::PerfMonCpu::StartMonitor(miCpuMonitor);
    mSoundScene.Update();
    CgsSound::Logic::StateManager::UpdateParams(af32DeltaTime);

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    BrnSound::Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        lpVehicles = lpInput->GetVehicleInterface();
    if (lpVehicles->IsPlayerCarActive())
    {
        const EActiveRaceCarIndex lePlayer = lpVehicles->GetPlayerActiveRaceCarIndex();
        const u32 luPlayer = static_cast<u32>(lePlayer);
        if (GetLoadedAssetId(luPlayer) != 0 &&
            IsDesiredEntryPlayer(luPlayer) && !IsAssetAttached(luPlayer))
        {
            CgsSound::Logic::State* lpState = GetFreeState(0);
            if (lpState)
            {
                VehicleState::AttachInfo lInfo;
                lInfo.Construct(GetLoadedAssetId(luPlayer),
                    const_cast<BrnResource::VehicleListEntry*>(GetLoadedVehicleEntry(luPlayer)),
                    luPlayer);
                lpState->Attach(&lInfo);
            }
        }
    }
    CgsDev::PerfMonCpu::StopMonitor(miCpuMonitor);
}

} // namespace Vehicles
} // namespace BrnSound
