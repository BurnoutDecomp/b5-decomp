#include "GameSource/Sound/Collision/BrnCollisionStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"   // CgsSceneManager::EntityId::SetPartIndex (glass / hinge part ids)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // Prepare: the "Collisions" CPU monitor
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"   // DeformationState (RaceCarCache::Update)
#include "GameSource/Sound/Collision/BrnCollisionState.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/World/BrnEntityTypes.h"   // BrnWorld::E_ENTITYTYPE_* (the car-part contact builder)
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.h"   // Traffic::TrafficStateManager::TrafficClassToSize
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

// =============================================================================
// BrnSound::Logic::Collision::CollisionStateManager -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// This canonical home brings up the collision/crash manager's resource binding,
// contact import, material/bin resolution, state allocation, and authored playback.
//
// Sources:
//   CollisionStateManager::CreateObject  @ 0x82701FA8  (real)
//   CollisionStateManager::Prepare       @ 0x826F8B78
//   CollisionStateManager::ctor          @ 0x826FFAC0
//   CollisionStateManager::dtor          @ 0x826FFD48
//   CollisionStateManager::ResourcesAreReady @ 0x826D3788
//   CollisionStateManager::Notify        @ 0x826F8E68
// GetTypeInfo / GetTypeName / GetStaticTypeInfo / GetResourceRegistrar are
// reconstructed from the established in-tree RTTI pattern + the sibling
// BrnEffectObject::GetResourceRegistrar @ 0x82696850. GetTypeName returns the
// "CollisionStateManager" literal (off_82F2F950, the tag CreateObject's operator
// new uses).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

namespace
{
bool CollisionAudioDiagEnabled()
{
    static const bool sbEnabled = std::getenv("BRN_COLLISION_AUDIO_DIAG") != nullptr;
    return sbEnabled;
}
}

// ---------------------------------------------------------------------------
// CollisionStateManager::CollisionStateManager()  ctor @ 0x826FFAC0  (HEAVY)
//
//   CgsSound::Logic::StateManager::StateManager();           ; base ctor
//   *(a1+144) = off_820AB608;                                ; (transient) +0x90 vtable
//   *a1       = off_820B844C;                                ; primary vtable @ +0
//   *(a1+144) = off_820B8444;                                ; IResourceRequester vtable @ +0x90
//   short_65536_::SelectionHistory_512(a1 + 0x8B0);          ; a SelectionHistory<512>
//   <zero a 32-entry table @ +0x1300 (stride 28)>
//   <zero a 500-entry table @ +0x1670 (stride 16)>
//   <zero a 16-entry table @ +0x1E69 (stride 48)>
//   <64x: vector-construct BaseCollisionGenerator arrays @ +0x2170 (16+4 each, stride 160)>
//   <64x: vector-construct BaseCollisionGenerator arrays @ +0x4990 (4+16 each, stride 224)>
//   <seed scalar block @ +0x81A0..>
//   <build crash Content sub-objects @ +0x8210.. ({&off_820B3250,0,0})>
//   Attrib::Gen::crashbinlist::crashbinlist(a1 + 0x8234, 0, 0);
//   Attrib::Gen::propscrashbinlist::propscrashbinlist(a1 + 0x8244, 0, 0);
//   Attrib::Gen::proptomaterialmappings::proptomaterialmappings(a1 + 0x8254, 0, 0);
//   <seed tail @ +0x8264..>
//   return a1;
//
// The host object keeps the same named runtime state needed by the recovered paths;
// console absolute offsets are intentionally not imposed on the 64-bit layout.
// ---------------------------------------------------------------------------
CollisionStateManager::CollisionStateManager()
    : BrnSound::Logic::BrnStateManager()
    , maBinLoopupCache()          // 0x826FFB00 / 0x826FFB04: `stw 0` to both muEntryCounts
    , maSelectionHistory()
    , maPropToMaterialMappings()
    , maInputCollision()
    , maOutputCollision()
    , mCameraInfo()
    , mu32InputCollisionCount(0)
    , mu32OutputCollisionCount(0)
    , mFrameInformation()
    , maScrapeHistory()
    , mScrapesCsisInterface()
    , mScrapesAemsBank()
    , mCollisionSplicerBank()
    , mCrashBinList()
    , mPropsCrashBinList()
    , mPropMaterialMappings()
    , mbResourcesAreLoaded(false)
    , mbBoundToProps(false)
    , mPropDataResourceHandle(CgsResource::NULLResourceHandle)
    , mx32CameraBinFlags(1)
    , mx32GameModeBinFlags(1)
{
}

// ---------------------------------------------------------------------------
// CollisionStateManager::~CollisionStateManager()  @ 0x826FFD48  (the X360 `vector deleting destructor`)
//
//   Attrib::Instance::~Instance(a1 + 0x8234);   ; proptomaterialmappings / crash tables
//   Attrib::Instance::~Instance(a1 + 0x8224);
//   Attrib::Instance::~Instance(a1 + 0x8214);
//   a1[8330] = &off_820B3250; <drop a refcounted CgsSound/Playback/CgsObject (refcount @ +4)>
//   a1[8327] = &off_820B3250; if (a1[8328]) CgsSound::Playback::Object::Release(...);
//   a1[8324] = &off_820B3250; if (a1[8325]) CgsSound::Playback::Object::Release(...);
//   *a1 = off_820B66E4;
//   CgsSound::Logic::StateManager::RegisteredContent_4_int_::~ObjectPool(a1 + 12);   ; base pool teardown
//   *a1 = &off_820AA820;                                                             ; MemBase vtable
//
// The generated attributes, Content handles, histories, and base pool are RAII members
// on PC, so their recovered teardown order is synthesized by the host compiler.
// ---------------------------------------------------------------------------
CollisionStateManager::~CollisionStateManager()
{
}

// ---------------------------------------------------------------------------
// CollisionStateManager::CreateObject(u32)  @ 0x82701FA8   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(33408, "CollisionStateManager", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(33408, "CollisionStateManager", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 33408-byte (0x8280) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "CollisionStateManager" (off_82F2F950) and placement-
// constructs a CollisionStateManager into it. Both arms call the SAME size+ctor; the
// `a1` argument only selects the operator-new flavour (0/1). The factory CreateStateMan
// @ 0x826A5B60 calls this as createObject(0).
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model operator
// new(size, tag, flavour) -- the sound allocator (off_82FFB954) is not homed in this
// group -- so a faithful placement-new through that allocator is not yet expressible.
// This reconstruction uses the host `new` (global operator new, NOT the sound
// allocator); the observable result -- a constructed CollisionStateManager* (or null)
// handed to the factory -- matches. Replace with the sound-allocator placement-new
// once MemBase::operator new is homed. The 33408-byte size is the X360 0x8280; on the
// 64-bit host the real object differs in size (and is FAR smaller here -- the ~33KB of
// collision state is the deferred pad), so the literal is documentation only and is
// NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* CollisionStateManager::CreateObject( u32 /*luType*/ )
{
    return new CollisionStateManager();
}

// ---------------------------------------------------------------------------
// CollisionStateManager::GetStaticTypeInfo()  (RTTI descriptor)
//
// Mirrors the in-tree GetStaticTypeInfo convention (CgsStateManager.cpp:230). A
// function-local static ClassTypeInfo<StateManager> seeded with (ObjectID, typeName,
// baseTypeInfo, createObject) so the factory CreateStateMan can match
// descriptor->ObjectID and call ->createObject.
//
// DecFIGS static initialization @ 0x85FA1C pins this manager's ObjectID to 5 and
// its base descriptor to StateManager::GetStaticTypeInfo().
//
// FLAG (registry hookup deferred): the minimal CgsSound::Logic::StateManager view
// pulled via BrnStateManager.h (this TU's base) does NOT declare
// AddToClassTypeInfoArray (full CgsStateManager.h view only, ODR-incompatible with
// BrnStateManager.h, not co-includable here). The descriptor is produced here but its
// insertion into the static registry (dword_82FFBC58) must be done by a registration
// site using the full StateManager view (the conductor-owned CreateStateMan TU).
// &CreateObject is an ABI-compatible StateManager*(*)(u32) across both views.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* CollisionStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        5,                          // ObjectID (PS3 DecFIGS static-init 0x85FA1C: CollisionStateManager=5)
        "CollisionStateManager",    // typeName
        CgsSound::Logic::StateManager::GetStaticTypeInfo(), // baseTypeInfo (PS3 0x85FA1C: =StateManager::GetStaticTypeInfo())
        &CollisionStateManager::CreateObject // createObject
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
// ObjectID RESOLVED (PS3 DecFIGS static-init 0x85FA1C): CollisionStateManager::sTypeInfo
// .ObjectID = 5. The descriptor comes from GetStaticTypeInfo() (seeded with that id and
// baseTypeInfo = StateManager::GetStaticTypeInfo()), so this registration lands the real
// id. NOTE (2026-08-25): this TU IS in the game build -- the registration runs at
// static-init and CreateStateManagers constructs this manager at boot.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpCollisionStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            CollisionStateManager::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// CollisionStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* CollisionStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// CollisionStateManager::GetTypeName() const
//   The X360 leaf GetTypeName loads the tag string (off_82F2F950) used by
//   CreateObject's operator new -> returns the literal "CollisionStateManager".
// ---------------------------------------------------------------------------
const char* CollisionStateManager::GetTypeName() const
{
    return "CollisionStateManager";
}

// ---------------------------------------------------------------------------
// CollisionStateManager::Prepare()  @ 0x826F8B78   (vtable +0x0C)
//
// X360 body: a switch on the +0x24 prepare-state (cases 0/5 -> 0, 1, 2, 3, 4):
//   state 1: SetCollisionBinList(global-data keys) (0x826F8BF8), then LoadAsset the
//            collision splice bundle and the scrape patch bundle (0x826F8C18 / 0x826F8C30);
//   state 2: wait for ResourcesAreReady (mbResourcesAreLoaded, 0x826F8C44); THEN construct
//            each of the three Contents that has no object yet (0x826F8C64 / 0x826F8CA0 /
//            0x826F8CDC) -- ScrapesCsis and ScrapePatchBank.abi on dword_83008664 ==
//            MakeHash("~AemsFactory::SK_NAME~") (CRT thunk 0x82C65790), the collision splice
//            bank on dword_83008404 / dword_83005F24 (the splicer factory / "CollisionSpliceBank");
//            wait until all three are loaded; miCpuMonitor (+0x94) =
//            PerfMonCpu::AddMonitor("Collisions", 14, 0, 1.0f, 0, 1) (0x826F8E00..0x826F8E20);
//   state 3: if (!StateManager::PrepareStates(3, 7, 0)) return 0;
//   state 4: return 1;
//
// The Content construction lives HERE on the console, not in ResourcesAreReady (which only
// builds the bin lookup caches and raises mbResourcesAreLoaded).
// ---------------------------------------------------------------------------
bool CollisionStateManager::Prepare()
{
    switch (GetPrepareState())
    {
    case E_PREPARE_NONE:
    case E_PREPARE_RELEASED:
        mePrepareState = E_PREPARE_NONE;
        // fall through
    case E_PREPARE_BEGIN:
        mePrepareState = E_PREPARE_BEGIN;
        {
            BrnSound::Module::SoundLogicModule* lpModule =
                static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
            CGS_ASSERT(lpModule != nullptr, "lpSoundLogicModule");
            if (!lpModule)
                return false;
            const Attrib::Gen::burnoutglobaldata& lrGlobalData =
                lpModule->GetGlobalData();
            SetCollisionBinList(lrGlobalData.CollisionCrashBinListKey(),
                                lrGlobalData.PropsCrashBinListKey(),
                                lrGlobalData.PropToMaterialMappingsKey());
        }
        LoadAsset("Sound\\Splicer\\CollisionSpliceBank.bundle",
                  "CollisionSpliceBank", ResourceRegistrar::E_DATA);
        LoadAsset("SOUND\\AEMS\\SCRAPEPATCHBANK.BUNDLE", nullptr,
                  ResourceRegistrar::E_DATA);
        // fall through
    case E_PREPARE_UPDATING:
    {
        mePrepareState = E_PREPARE_UPDATING;
        if (!mbResourcesAreLoaded)                                                  // 0x826F8C44
            return false;

        CgsSound::Logic::Module* lpModule = GetLogicModule();                      // lwz 0x2C(r31)
        const u32 luAemsFactory = static_cast<u32>(
            CgsSound::Playback::AemsFactorySkName().GetValue());                   // dword_83008664
        if (!mScrapesCsisInterface.IsCreated())                                     // 0x826F8C64
            mScrapesCsisInterface.Construct(
                lpModule, luAemsFactory,
                static_cast<u32>(CgsSound::Playback::Name::MakeHash("ScrapesCsis")));
        if (!mScrapesAemsBank.IsCreated())                                          // 0x826F8CA0
            mScrapesAemsBank.Construct(
                lpModule, luAemsFactory,
                static_cast<u32>(CgsSound::Playback::Name::MakeHash("ScrapePatchBank.abi")));
        if (!mCollisionSplicerBank[E_COLLISION_SPLICE_BANK_COLLISION].IsCreated())  // 0x826F8CDC
            mCollisionSplicerBank[E_COLLISION_SPLICE_BANK_COLLISION].Construct(
                lpModule,
                static_cast<u32>(CgsSound::Playback::Name::MakeHash("~SplicerFactory::SK_NAME~")),  // dword_83008404
                static_cast<u32>(CgsSound::Playback::Name::MakeHash("CollisionSpliceBank")));       // dword_83005F24

        // 0x826F8D04..0x826F8DFC: the splice bank's state is read first and kept, then the
        // scrape CSIS interface, the scrape patch bank, and the kept splice-bank result.
        const bool lbSplicerBankLoaded =
            mCollisionSplicerBank[E_COLLISION_SPLICE_BANK_COLLISION].IsLoaded();
        if (!mScrapesCsisInterface.IsLoaded() ||
            !mScrapesAemsBank.IsLoaded() ||
            !lbSplicerBankLoaded)
            return false;

        miCpuMonitor = CgsDev::PerfMonCpu::AddMonitor("Collisions", 14, 0, 1.0, 0, 1);  // 0x826F8E1C
    }
        // fall through
    case E_PREPARE_STATES:
        mePrepareState = E_PREPARE_STATES;
        if (!PrepareStates(3, 7, 0))
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
// CollisionStateManager::ResourcesAreReady() @ 0x826D3788
// (IResourceRequester completion callback; entered on the IResourceRequester sub-object,
// which is why the console reaches the primary with `addi r3, r31, -0x90` for
// BuildPropToMaterialTable at 0x826D3828)
//
//   0x826D37A8  lbz mbResourcesAreLoaded ; bne -> skip
//   0x826D37C0  maBinLoopupCache[0].Build<crashbinlist, crashbin>(mCrashBinList)
//   0x826D37D0  maBinLoopupCache[1].Build<propscrashbinlist, propscrashbin>(mPropsCrashBinList)
//   0x826D37E0  stb 1, mbResourcesAreLoaded           ; on EVERY call, outside the if
//   0x826D37E4  if (mbBoundToProps) { GetAsset(0, CgsIDUnCompress(0xA773D7113DF454BF))
//                                     -> mPropDataResourceHandle; BuildPropToMaterialTable(); }
// No Content is constructed here -- Prepare state 2 does that once this flag is up.
// ---------------------------------------------------------------------------
void CollisionStateManager::ResourcesAreReady()
{
    if (!mbResourcesAreLoaded)
    {
        maBinLoopupCache[InputCollision::E_REGULAR]
            .Build<Attrib::Gen::crashbinlist, Attrib::Gen::crashbin>(mCrashBinList);
        maBinLoopupCache[InputCollision::E_PROP]
            .Build<Attrib::Gen::propscrashbinlist, Attrib::Gen::propscrashbin>(mPropsCrashBinList);

        // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): the two caches the bin
        // selection reads, as built.
        if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
        {
            *CgsDev::Log::gpDebugPrint
                << "[collision-audio] bin caches built regular="
                << static_cast<s32>(maBinLoopupCache[InputCollision::E_REGULAR].GetEntryCount())
                << "/" << static_cast<s32>(mCrashBinList.mNumCrashBins())
                << " prop="
                << static_cast<s32>(maBinLoopupCache[InputCollision::E_PROP].GetEntryCount())
                << "/" << static_cast<s32>(mPropsCrashBinList.mNumCrashBins()) << "\n";
            // Every entry's material pair, once: what a collision's two materials must meet -- and,
            // for the regular bins, the other gates SelectCollisionBin walks (the action and
            // orientation masks, the distance and impulse bands).
            for (u32 luPipeline = 0; luPipeline < InputCollision::E_MAX_PIPELINES; ++luPipeline)
            {
                const BinLookupCache& lrCache = maBinLoopupCache[luPipeline];
                for (u32 luEntry = 0; luEntry < lrCache.GetEntryCount(); ++luEntry)
                {
                    char lacLine[256];
                    if (luPipeline == InputCollision::E_REGULAR)
                    {
                        const Attrib::Gen::crashbin lBin(mCrashBinList.GetCrashBinCollectionKey(luEntry), nullptr);
                        std::snprintf(lacLine, sizeof(lacLine),
                                      "[collision-audio] cache entry pipeline=%u %u matA=0x%016llx matB=0x%016llx"
                                      " action=%u orient=%u distance=%g..%g impulse=%g..%g\n",
                                      luPipeline, luEntry,
                                      static_cast<unsigned long long>(lrCache.GetEntry(luEntry).mx64MaterialA),
                                      static_cast<unsigned long long>(lrCache.GetEntry(luEntry).mx64MaterialB),
                                      lBin.IsValid() ? static_cast<u32>(lBin.mAction()) : 0u,
                                      lBin.IsValid() ? static_cast<u32>(lBin.mOrientation()) : 0u,
                                      lBin.IsValid() ? static_cast<double>(lBin.DistanceFactor_Min()) : 0.0,
                                      lBin.IsValid() ? static_cast<double>(lBin.DistanceFactor_Max()) : 0.0,
                                      lBin.IsValid() ? static_cast<double>(lBin.PhysicsImpulseNormalization_MIN()) : 0.0,
                                      lBin.IsValid() ? static_cast<double>(lBin.PhysicsImpulseNormalization_MAX()) : 0.0);
                    }
                    else
                    {
                        std::snprintf(lacLine, sizeof(lacLine),
                                      "[collision-audio] cache entry pipeline=%u %u matA=0x%016llx matB=0x%016llx\n",
                                      luPipeline, luEntry,
                                      static_cast<unsigned long long>(lrCache.GetEntry(luEntry).mx64MaterialA),
                                      static_cast<unsigned long long>(lrCache.GetEntry(luEntry).mx64MaterialB));
                    }
                    *CgsDev::Log::gpDebugPrint << lacLine;
                }
            }
        }
    }
    mbResourcesAreLoaded = true;

    // [NOTE] the prop branch below keeps two host guards the console lacks (the handle must be
    // NULL to re-fetch, and a NULL fetch skips the table build); see the FX-CRASHSND log.
    if (mbBoundToProps &&
        mPropDataResourceHandle == CgsResource::NULLResourceHandle)
    {
        char lacResourceName[KI_CGSID_STRING_LEN] = {};
        CgsIDUnCompress(0xA773D7113DF454BFull, lacResourceName);
        mPropDataResourceHandle = GetAsset(nullptr, lacResourceName);
        if (mPropDataResourceHandle != CgsResource::NULLResourceHandle)
        {
            BuildPropToMaterialTable();
            if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
            {
                u32 luValidMappings = 0;
                for (u32 luIndex = 0; luIndex < 500u; ++luIndex)
                    luValidMappings += maPropToMaterialMappings[luIndex].mbValid ? 1u : 0u;
                *CgsDev::Log::gpDebugPrint
                    << "[collision-audio] prop resource bound mappings="
                    << static_cast<s32>(luValidMappings) << "\n";
            }
        }
    }
}

void CollisionStateManager::Notify(const CgsSound::Io::MessageHeader* apkMessage)
{
    CGS_ASSERT(apkMessage != nullptr, "lpkMessage");
    if (!apkMessage)
        return;

    if (apkMessage->GetEventId() ==
            BrnSound::E_SOUNDMESSAGE_COLLISION_BIND_TO_PROPS &&
        !mbBoundToProps)
    {
        char lacResourceName[KI_CGSID_STRING_LEN] = {};
        CgsIDUnCompress(0xA773D7113DF454BFull, lacResourceName);
        LoadAsset(lacResourceName, E_PHYSICS_DATA_POOL,
                  ResourceRegistrar::E_DATA);
        mbBoundToProps = true;
    }
}

// ---------------------------------------------------------------------------
// CollisionStateManager::GetResourceRegistrar()  (IResourceRequester slot 1)
//
// Recovered semantically from the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850: load this->mpLogicModule (+0x2C), tail-call the IResourceRequester
// slot-1 of the module's embedded ResourceRegistrar. The state-manager leaves share
// the +0x2C module back-pointer (stamped by CreateStateMan).
//
// ---------------------------------------------------------------------------
BrnSound::Logic::ResourceRegistrar& CollisionStateManager::GetResourceRegistrar()
{
    return BrnSound::Logic::BrnStateManager::GetResourceRegistrar();
}

// ---------------------------------------------------------------------------
// CollisionStateManager::FindInScrapeHistory(const ScrapeInfo&)  @ 0x826889E0
//   DWARF (BrnCollisionStateManager.h:880): non-const member returning ScrapeInfo*.
//
// Linear scan of the 16-slot scrape history (maScrapeHistory, DWARF h:639): for each
// slot, the per-element ScrapeInfo::mbValid flag byte (+0x29) gates the comparison --
// if the flag is clear, the slot is skipped WITHOUT calling operator== (mirrors the asm
// short-circuit `!*(v5+41) || !operator==(...)`). The comparison is ScrapeInfo::
// operator== (committed BrnCollisionDataStructures.cpp). First slot where mbValid is set
// AND operator== returns true is returned; 16 misses returns nullptr.
//
// FLAG: maScrapeHistory is modelled with the COMMITTED ScrapeInfo (which carries mbValid
// + operator==); the DWARF's full 48-byte ScrapeInfo shape (EntityId pair, CollisionTag,
// eOrientation, etc.) is a deferred richer form -- this is a semantic-parity match on the
// scan gate + equality, not a byte-exact element layout.
// ---------------------------------------------------------------------------
BrnSound::Logic::Collision::ScrapeInfo*
CollisionStateManager::FindInScrapeHistory( const BrnSound::Logic::Collision::ScrapeInfo& rScrapeInfo )
{
    for ( u32 luIndex = 0; luIndex < 16u; ++luIndex )
    {
        BrnSound::Logic::Collision::ScrapeInfo& rSlot = maScrapeHistory[luIndex];

        // asm: `!*(v5+41)` (lbz r11,0x29(r30)) short-circuits operator== when the slot's
        // mbValid flag byte (+0x29) is clear.
        if ( rSlot.mbValid && ( rSlot == rScrapeInfo ) )
        {
            return &rSlot;
        }
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// CollisionStateManager::PlayCollision(OutputCollision*)  @ 0x82704028
//
// Allocates a collision state, chooses a sample through the correct authored bin type,
// then attaches the resolved output. A bin with no eligible sample is a successful
// no-op, matching ARTIST.
// ---------------------------------------------------------------------------
int CollisionStateManager::PlayCollision(OutputCollision* lpCollision)
{
    CGS_ASSERT(lpCollision != nullptr, "lpCollision");
    if (!lpCollision || lpCollision->miSampleID == -1)
        return 1;

    CollisionState* lpState = static_cast<CollisionState*>(GetFreeState(lpCollision));
    if (!lpState)
        return 0;

    switch (lpCollision->mePipeline)
    {
    case InputCollision::E_REGULAR:
        GetRandomSampleID<Attrib::Gen::crashbin>(*lpCollision);
        break;
    case InputCollision::E_PROP:
        GetRandomSampleID<Attrib::Gen::propscrashbin>(*lpCollision);
        break;
    default:
        CGS_ASSERT(false, "lCollision.mePipeline < InputCollision::E_MAX_PIPELINES");
        return 0;
    }

    if (lpCollision->miSampleID < 0)
    {
        // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): a bin was chosen but its
        // sample pick came back empty (the state stays free).
        if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
        {
            static u32 suNoSamplePrintCount = 0;
            if (suNoSamplePrintCount++ < 32u)
            {
                char lacLine[160];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[collision-audio] no sample pipeline=%d bin=%d size=%d action=%d mat=%llx/%llx\n",
                              static_cast<s32>(lpCollision->mePipeline), static_cast<s32>(lpCollision->miBinIndex),
                              static_cast<s32>(lpCollision->meSize), static_cast<s32>(lpCollision->meAction),
                              static_cast<unsigned long long>(lpCollision->maMaterial[0]),
                              static_cast<unsigned long long>(lpCollision->maMaterial[1]));
                *CgsDev::Log::gpDebugPrint << lacLine;
            }
        }
        return 1;
    }

    if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
    {
        static u32 suPrintCount = 0;
        if (suPrintCount++ < 64u)
        {
            char lacLine[224];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[collision-audio] play pipeline=%d bin=%d size=%d sample=%d orient=%d impulse=%g "
                          "action=%d mat=%llx/%llx\n",
                          static_cast<s32>(lpCollision->mePipeline), static_cast<s32>(lpCollision->miBinIndex),
                          static_cast<s32>(lpCollision->meSize), lpCollision->miSampleID,
                          static_cast<s32>(lpCollision->meOrientation),
                          static_cast<double>(lpCollision->mNormalizedImpulse.x),
                          static_cast<s32>(lpCollision->meAction),
                          static_cast<unsigned long long>(lpCollision->maMaterial[0]),
                          static_cast<unsigned long long>(lpCollision->maMaterial[1]));
            *CgsDev::Log::gpDebugPrint << lacLine;
        }
    }

    lpState->Attach(lpCollision);
    lpState->SetLifetime(CollisionState::E_COLLISION);
    return 1;
}

void CollisionStateManager::SetCollisionBinList(
    u64 luCollisionBinListKey,
    u64 luPropsCollisionBinListKey,
    u64 luPropsMappingKey)
{
    mCrashBinList.ChangeWithDefault(luCollisionBinListKey);
    CGS_ASSERT(mCrashBinList.mNumCrashBins() != 0, "mCrashBinList.mNumCrashBins() != 0");
    for (u32 luIndex = 0; luIndex < mCrashBinList.mNumCrashBins(); ++luIndex)
    {
        Attrib::Gen::crashbin lBin(
            mCrashBinList.GetCrashBinCollectionKey(luIndex), nullptr);
        const s32 liBank = SelectBin(0, lBin.mSpliceBankAsset(), 0, 0, 0);
        CGS_ASSERT(liBank >= E_COLLISION_SPLICE_BANK_COLLISION &&
                   liBank < E_COLLISION_SPLICE_BANK_MAX,
                   "leSpliceBankType < E_COLLISION_SPLICE_BANK_MAX");
    }

    mPropsCrashBinList.ChangeWithDefault(luPropsCollisionBinListKey);
    CGS_ASSERT(mPropsCrashBinList.mNumCrashBins() != 0,
               "mPropsCrashBinList.mNumCrashBins() != 0");
    for (u32 luIndex = 0; luIndex < mPropsCrashBinList.mNumCrashBins(); ++luIndex)
    {
        Attrib::Gen::propscrashbin lBin(
            mPropsCrashBinList.GetCrashBinCollectionKey(luIndex), nullptr);
        const s32 liBank = SelectBin(0, lBin.mSpliceBankAsset(), 0, 0, 0);
        CGS_ASSERT(liBank >= E_COLLISION_SPLICE_BANK_COLLISION &&
                   liBank < E_COLLISION_SPLICE_BANK_MAX,
                   "leSpliceBankType < E_COLLISION_SPLICE_BANK_MAX");
    }

    mPropMaterialMappings.ChangeWithDefault(luPropsMappingKey);
}

void CollisionStateManager::BuildPropToMaterialTable()
{
    const u32 luMappingCount = mPropMaterialMappings.MappingCount();
    CGS_ASSERT(luMappingCount < 500u, "mPropMaterialMappings.MappingCount() < KU_MAX_PROP_TYPES");

    for (u32 luIndex = 0; luIndex < 500u; ++luIndex)
        maPropToMaterialMappings[luIndex] = PropToMaterialMapping();

    const BrnPhysics::Props::PropPhysicsDataHeader* lpPropPhysics = nullptr;
    if (mPropDataResourceHandle.mpResourceMemory)
    {
        lpPropPhysics = *reinterpret_cast<
            BrnPhysics::Props::PropPhysicsDataHeader* const*>(
                mPropDataResourceHandle.mpResourceMemory);
    }
    CGS_ASSERT(lpPropPhysics != nullptr, "lpPropPhysicsData");
    if (!lpPropPhysics)
        return;

    for (u32 luMapping = 0; luMapping < luMappingCount; ++luMapping)
    {
        const u64 luCgsId = mPropMaterialMappings.CgsIds(luMapping);
        for (u32 luType = 0;
             luType < lpPropPhysics->GetNumberOfPropTypes(); ++luType)
        {
            const BrnPhysics::Props::PropTypeData* lpType =
                lpPropPhysics->GetType(luType);
            if (lpType && lpType->GetResourceId().GetHash() == luCgsId)
            {
                CGS_ASSERT(!maPropToMaterialMappings[luType].mbValid,
                           "!maPropToMaterialMappings[luType].mbValid");
                maPropToMaterialMappings[luType].muMaterialIndex =
                    mPropMaterialMappings.MaterialIndices(luMapping);
                maPropToMaterialMappings[luType].mbValid = true;
                break;
            }
        }
    }
}

bool CollisionStateManager::MapPropTypeToMaterial(
    u16 luPropType, u64& lruMaterial) const
{
    if (luPropType >= 500u || !maPropToMaterialMappings[luPropType].mbValid)
        return false;

    const u16 luMaterialIndex =
        maPropToMaterialMappings[luPropType].muMaterialIndex;
    CGS_ASSERT(luMaterialIndex < 64u, "luMaterialIndex < 64");
    lruMaterial = 1ull << luMaterialIndex;
    return true;
}

// ---------------------------------------------------------------------------
// KAE_TRAFFIC_CAR_SIZE_TO_MATERIAL_TABLE (DWARF cpp:124) -- dword_820AA79C = { 0x8, 0x800000,
// 0x1000000 }: a small, medium or large traffic car.
// ---------------------------------------------------------------------------
namespace
{
const EeMaterialType KAE_TRAFFIC_CAR_SIZE_TO_MATERIAL_TABLE[Traffic::E_MAX_SIZES] =
{
    AttribSys::Enums::eMaterialType::TrafficCar,
    AttribSys::Enums::eMaterialType::TrafficCarMedium,
    AttribSys::Enums::eMaterialType::TrafficCarLarge,
};

// An EntityId's owner byte and 14-bit entity index (`srwi 24` / `extrwi 14,8` throughout).
u32 GetEntityOwner(EntityId lEntityId) { return lEntityId.muValue >> 24; }
u32 GetEntityIndex(EntityId lEntityId) { return (lEntityId.muValue >> 10) & 0x3FFFu; }

const u32 KU_ENTITY_OWNER_WORLD    = 0;
const u32 KU_ENTITY_OWNER_RACE_CAR = 1;
const u32 KU_ENTITY_OWNER_TRAFFIC  = 2;
const u32 KU_ENTITY_OWNER_PROP     = 3;
}

// ---------------------------------------------------------------------------
// MapEntityIdToMaterial(EntityId, s32, const LogicInputBuffer&)  @ 0x826A0CF8  (DWARF cpp:3593)
//
//   srwi r11, id, 24 ; cmplwi 1
//   owner 0  -> World (0x10)                                        0x826A0DB0
//   owner 1  -> index == player ? PlayerCar (2) : AiCar (4)          0x826A0D94..0x826A0DA8
//   owner 2  -> GetTrafficOutputInterface() @0x82694DD8, GetTrafficEntityIndex(index) @0x82681EC8;
//               a found entity's muVehicleClass (+0x4A) -> TrafficStateManager::TrafficClassToSize
//               @0x82683290 (assert < E_MAX_SIZES, cpp:3735); not found -> size 0; return
//               KAE_TRAFFIC_CAR_SIZE_TO_MATERIAL_TABLE[size]                0x826A0D30..0x826A0D8C
//   owner 3+ -> Nothing (1)                                            0x826A0D28
//
// The PC used to classify traffic by the PhysicalTrafficState crash flags (fatally crashing ->
// large, deforming -> medium): the console never reads them here -- the material is the car's
// SIZE, from the traffic sound output interface.
// ---------------------------------------------------------------------------
EeMaterialType MapEntityIdToMaterial(EntityId lEntityId, s32 liPlayerIndex,
                                     const LogicInputBuffer& lInput)
{
    const u32 luOwner = GetEntityOwner(lEntityId);
    if (luOwner < KU_ENTITY_OWNER_RACE_CAR)
        return AttribSys::Enums::eMaterialType::World;
    if (luOwner == KU_ENTITY_OWNER_RACE_CAR)
        return static_cast<s32>(GetEntityIndex(lEntityId)) == liPlayerIndex
                   ? AttribSys::Enums::eMaterialType::PlayerCar
                   : AttribSys::Enums::eMaterialType::AiCar;
    if (luOwner > KU_ENTITY_OWNER_TRAFFIC)
        return AttribSys::Enums::eMaterialType::Nothing;

    Traffic::ETrafficSize leSize = Traffic::E_SMALL;
    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* lpEntity =
        lInput.GetTrafficOutputInterface().GetTrafficEntityIndex(
            static_cast<u16>(GetEntityIndex(lEntityId)));
    if (lpEntity)
    {
        leSize = Traffic::TrafficStateManager::TrafficClassToSize(lpEntity->muVehicleClass);
        CGS_ASSERT(leSize < Traffic::E_MAX_SIZES,
                   "leSize < BrnSound::Logic::Traffic::E_MAX_SIZES");
    }
    return KAE_TRAFFIC_CAR_SIZE_TO_MATERIAL_TABLE[leSize];
}

// ---------------------------------------------------------------------------
// KV_DIRECTION_BIAS (DWARF cpp:88) -- unk_83008160 = (1, 1, 1, 0), written by the CRT thunk
// 0x82C63308..0x82C63338 from flt_82001C98 (1.0f). The per-axis weight of the box-face distances.
// ---------------------------------------------------------------------------
Vector3 KV_DIRECTION_BIAS = { 1.0f, 1.0f, 1.0f, 0.0f };

namespace
{
    // One lane of `vminfp vD, vA, vB` (AltiVec PEM): a NaN in EITHER operand comes back (vA's when
    // both are), and -0 is the smaller zero. std::min(a, b) instead keeps `a` when only `b` is NaN.
    // The side distance below is `vminfp v10, v7, v8` at 0x8269EF10 (crash parity FX-GATE).
    // Not modelled: the VMX non-Java mode's flush of denormal operands to zero.
    f32 VminfpLane(f32 lfA, f32 lfB)
    {
        if (lfA != lfA) return lfA;
        if (lfB != lfB) return lfB;
        if (lfA == lfB) return std::signbit(lfA) ? lfA : lfB;
        return (lfA < lfB) ? lfA : lfB;
    }
}

// ---------------------------------------------------------------------------
// MapPositionToOrientationUsingBox  @ 0x8269ED18  (DWARF cpp:190)
//
// Bring the contact point into the car's frame: the three row vectors of the cached transform
// are merged into columns (0x8269ED74..0x8269ED90 vmrglw/vmrghw), the translation term is built
// first (-T.z, -T.y, -T.x) and the point added after (x, y, z) -- 0x8269EDB8..0x8269EE10 -- and
// the COM offset is subtracted (0x8269EE14). Then the distance to each face of the deformed box,
// |local - face| (vandc of the sign bit) scaled by KV_DIRECTION_BIAS (0x8269EE2C..0x8269EEC0):
//   front |z - max.z|  back |z - min.z|  left |x - min.x|  right |x - max.x|
//   top   |y - max.y|  bottom |y - min.y|
// and the nearest face wins through a fixed chain of all-lanes `vcmpgtfp.` tests (a NaN compares
// false): Front, then Rear if nearer, then Side if nearer than EITHER side (the side distance is
// the smaller of the two, vminfp 0x8269EF10), then Roof, then Bottom (0x8269EE90..0x8269EF4C).
// The normal is read only by the debug print ("Normal:"), and the fourth vector not at all; the
// debug draw (dword_82FFB910) and print (dword_82FFB90C) have no writer.
// ---------------------------------------------------------------------------
AttribSys::Enums::eOrientation::eOrientation MapPositionToOrientationUsingBox(
    Vector3 lPosition, Vector3 /*lNormal*/, Matrix44Affine lTransform, Vector3 /*lUnused*/,
    Vector3 lComOffset, Vector3 lMin, Vector3 lMax)
{
    using namespace AttribSys::Enums::eOrientation;

    const Vector3* lapRows[3] = { &lTransform.Right(), &lTransform.Up(), &lTransform.At() };
    const Vector3& lrTranslation = lTransform.Pos();
    f32 lafLocal[3];
    for (u32 luAxis = 0; luAxis < 3u; ++luAxis)
    {
        const Vector3& lrRow = *lapRows[luAxis];
        f32 lfLocal = -lrTranslation.z * lrRow.z;
        lfLocal = -lrTranslation.y * lrRow.y + lfLocal;
        lfLocal = -lrTranslation.x * lrRow.x + lfLocal;
        lfLocal = lrRow.x * lPosition.x + lfLocal;
        lfLocal = lrRow.y * lPosition.y + lfLocal;
        lfLocal = lrRow.z * lPosition.z + lfLocal;
        lafLocal[luAxis] = lfLocal;
    }
    const f32 lfLocalX = lafLocal[0] - lComOffset.x;
    const f32 lfLocalY = lafLocal[1] - lComOffset.y;
    const f32 lfLocalZ = lafLocal[2] - lComOffset.z;

    const f32 lfFrontDist  = std::fabs(lfLocalZ - lMax.z) * KV_DIRECTION_BIAS.z;
    const f32 lfBackDist   = std::fabs(lfLocalZ - lMin.z) * KV_DIRECTION_BIAS.z;
    const f32 lfLeftDist   = std::fabs(lfLocalX - lMin.x) * KV_DIRECTION_BIAS.x;
    const f32 lfRightDist  = std::fabs(lfLocalX - lMax.x) * KV_DIRECTION_BIAS.x;
    const f32 lfTopDist    = std::fabs(lfLocalY - lMax.y) * KV_DIRECTION_BIAS.y;
    const f32 lfBottomDist = std::fabs(lfLocalY - lMin.y) * KV_DIRECTION_BIAS.y;

    eOrientation leOrientation = Front;
    f32 lfNearest = lfFrontDist;
    if (lfNearest > lfBackDist)
    {
        lfNearest = lfBackDist;
        leOrientation = Rear;
    }
    if (lfNearest > lfLeftDist || lfNearest > lfRightDist)
    {
        // 0x8269EF0C li r25, 2 ; 0x8269EF10 vminfp v10, v7, v8: a NaN side distance makes the nearest
        // distance NaN, so the Roof (0x8269EF18) and Bottom (0x8269EF38) vcmpgtfp. tests fail after it.
        leOrientation = Side;
        lfNearest = VminfpLane(lfLeftDist, lfRightDist);
    }
    if (lfNearest > lfTopDist)
    {
        lfNearest = lfTopDist;
        leOrientation = Roof;
    }
    if (lfNearest > lfBottomDist)
        leOrientation = Bottom;
    return leOrientation;
}

// ---------------------------------------------------------------------------
// MapPositionToOrientation  @ 0x8269F418  (DWARF cpp:318)
//
//   A not a race car (`srwi 24 ; cmplwi 1 ; bne`)             -> Front, false   0x8269F4F4
//   assert index < 8 (cpp:327)
//   node = lMgr.GetRaceCarCache().GetRaceCar(index)  (mgr+0xCF0, bl 0x82683068)
//   !node || !node->mbActive PREVIOUS (`lbz 0x11`)           -> Front, false
//   *out = UsingBox(position, normal, node->mTransform PREVIOUS (+0x60), 0, com (+0x00),
//                   min (+0xA0), max (+0xB0))                 0x8269F4A8..0x8269F4D4
//   Side against another race car (B's owner byte 1)          -> Front (`stw r11 = 1`)
//   return true
// The cache's DataPoints keep {current, previous}; RaceCarCache::Update runs at the head of
// UpdateResolver, so "previous" is the car as it stood before this frame's contacts. The leading
// matrix (the manager's mFrameInformation.mPlayerTransform) is never read -- r3 is overwritten
// by `addi r3, r30, 0xCF0`.
// ---------------------------------------------------------------------------
bool MapPositionToOrientation(Matrix44Affine /*lTransform*/, Vector3 lPosition, Vector3 lNormal,
                              EntityId lVehicleIdA, EntityId lVehicleIdB,
                              const CollisionStateManager& lMgr,
                              AttribSys::Enums::eOrientation::eOrientation& leOrientation)
{
    if (GetEntityOwner(lVehicleIdA) == KU_ENTITY_OWNER_RACE_CAR)
    {
        const u32 luIndex = GetEntityIndex(lVehicleIdA);
        CGS_ASSERT(luIndex < RaceCarCache::KU_MAX_NUM_RACE_CARS,
                   "lVehicleIdA.GetEntityIndex() < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
        const RaceCarCache::RaceCarCacheNode* lpCar = lMgr.GetRaceCarCache().GetRaceCar(luIndex);
        if (lpCar && lpCar->mbActive.GetPrevious())
        {
            const Vector3 lZero = { 0.0f, 0.0f, 0.0f, 0.0f };   // vspltisw v3, 0
            leOrientation = MapPositionToOrientationUsingBox(
                lPosition, lNormal, lpCar->mTransform.GetPrevious(), lZero,
                lpCar->mComOffset, lpCar->mMin, lpCar->mMax);
            if (leOrientation == AttribSys::Enums::eOrientation::Side &&
                GetEntityOwner(lVehicleIdB) == KU_ENTITY_OWNER_RACE_CAR)
            {
                leOrientation = AttribSys::Enums::eOrientation::Front;
            }
            return true;
        }
    }
    leOrientation = AttribSys::Enums::eOrientation::Front;
    return false;
}

// ---------------------------------------------------------------------------
// ScrapeInfo::ScrapeInfo(InputContactSpy, eOrientation, f32, f32)  (DWARF h:299; inlined -- the
// regular InputCollision builds it at sp+0x70 and block-copies it in, 0x826D3A60..0x826D3AD0):
// no relative velocity, the spy's two entities and B's collision tag, the stamp, the orientation,
// the intensity, not crashing, valid.
// ---------------------------------------------------------------------------
ScrapeInfo::ScrapeInfo(InputContactSpy lSpy,
                       AttribSys::Enums::eOrientation::eOrientation leOrientation,
                       f32 lfTimeStamp, f32 lfIntensity)
    : mEntityIdA(lSpy.mEntityIdA)
    , mEntityIdB(lSpy.mEntityIdB)
    , mfTimeStamp(lfTimeStamp)
    , mCollisionTagB(lSpy.mCollisionTagB)
    , meOrientation(leOrientation)
    , mfIntensity(lfIntensity)
    , mbCrashing(false)
    , mbValid(true)
{
    mRelativeVelocity.SetZero();
}

// ---------------------------------------------------------------------------
// InputCollision::SwapEntityIds  (DWARF h:474; inlined at 0x826D391C..0x826D3944): the two
// entities and the two contact points trade places; the normal and the stresses do not.
// ---------------------------------------------------------------------------
void InputCollision::SwapEntityIds(InputContactSpy& lSpy)
{
    std::swap(lSpy.mEntityIdA, lSpy.mEntityIdB);
    std::swap(lSpy.mPointOnA, lSpy.mPointOnB);
}

namespace
{
// ARTIST flt_830083E0 = KF_BIGGEST_THING_MASS (1600, 0x82F2CEFC) * KF_FASTEST_COLLISION (200,
// 0x82F2CEF8), computed by the CRT thunk 0x82C63230..0x82C63248: KF_BIGGEST_COLLISION_IN_SECOND
// (DWARF cpp:74), the stress-to-per-second normalisation of every contact builder.
const f32 KF_BIGGEST_COLLISION_IN_SECOND = 1600.0f * 200.0f;

f32 Dot3(const Vector3& lrA, const Vector3& lrB)
{
    return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;
}

VecFloat Splat(f32 lfValue)
{
    const VecFloat lSplat = { lfValue, lfValue, lfValue, lfValue };
    return lSplat;
}

// The impulse lane both contact builders store in maParameter[0]: 1 / (K * dt) FIRST (fmuls,
// fdivs), then |stress| (vmsum3fp + the two-step rsqrte refinement, a zero length selected to
// zero by vsel), multiplied in the vector unit. There is no guard on dt.
f32 NormalisedImpulse(const Vector3& lrNormalStress, f32 lfTimeStep)
{
    const f32 lfInverseSecond = 1.0f / (KF_BIGGEST_COLLISION_IN_SECOND * lfTimeStep);
    return lfInverseSecond * std::sqrt(Dot3(lrNormalStress, lrNormalStress));
}
}

// ---------------------------------------------------------------------------
// InputCollision::InputCollision(const CameraInfo&, CollisionStateManager&, const InputContactSpy&,
//                                const LogicInputBuffer&, f32, f32)   sub_826D3850 (DWARF h:405)
//
// The race-car and traffic contacts (ImportContactSpies 0x826DD090 / 0x826DD128):
//   0x826D3898..0x826D38D0  mScrapeInfo.mbValid = 0, mfPriorityAddition = 0.0, meAction =
//                           Collision, meOrientation = Front, mePipeline = E_REGULAR, mbCull = 0,
//                           mPosition = the ORIGINAL spy's point on A
//   0x826D38D8..0x826D3944  lSpyModified = the spy; two race cars are ordered by index (the
//                           higher index goes second -- SwapEntityIds)
//   0x826D3948..0x826D3A20  maParameter[0] = the normalised impulse (NormalisedImpulse)
//   0x826D3A24..0x826D3B48  MapPositionToOrientation from A's side; if A is not an active race
//                           car, the pair is reversed (maEntityID and the scrape entry take (B, A))
//                           and the orientation is taken from B's side at B's contact point
//   0x826D3B4C..0x826D3C00  the two materials, each `extsw` of MapEntityIdToMaterial (NO
//                           0x2000000000 bit -- Hex-Rays prints one, the instructions have none)
//                           with the player index read through GetVehicleInterface() (h:980)
//   0x826D3C04..0x826D3C50  maParameter[1] = |position - camera|^2, maParameter[2] =
//                           normalize(position - camera) . the camera's At row (camera info +0x20)
//   0x826D3C54..0x826D3C90  SloMoCrash culling: outside impact time False, a scrape already in the
//                           history less than 5 s old (flt_820ABCD8) culls this collision
// ---------------------------------------------------------------------------
InputCollision::InputCollision(const CameraInfo& lCamera, CollisionStateManager& lMgr,
                               const InputContactSpy& lSpy, const LogicInputBuffer& lInput,
                               f32 lfTimeStamp, f32 lfTimeStep)
    : maMaterial{0, 0}
    , maEntityID{{0}, {0}}
    , mfPriorityAddition(0.0f)
    , meAction(AttribSys::Enums::eAction::Collision)
    , meOrientation(AttribSys::Enums::eOrientation::Front)
    , mePipeline(E_REGULAR)
    , mbCull(false)
{
    mScrapeInfo.mbValid = false;
    mPosition = lSpy.mPointOnA;

    InputContactSpy lSpyModified = lSpy;
    if (GetEntityOwner(lSpyModified.mEntityIdA) == KU_ENTITY_OWNER_RACE_CAR &&
        GetEntityOwner(lSpyModified.mEntityIdB) == KU_ENTITY_OWNER_RACE_CAR &&
        GetEntityIndex(lSpyModified.mEntityIdA) > GetEntityIndex(lSpyModified.mEntityIdB))
    {
        SwapEntityIds(lSpyModified);
    }
    maEntityID[0] = lSpyModified.mEntityIdA;
    maEntityID[1] = lSpyModified.mEntityIdB;

    maParameter[0] = Splat(NormalisedImpulse(lSpyModified.mNormalStress, lfTimeStep));

    const BrnSound::Logic::FrameInformation& lrFrame = lMgr.GetFrameInformation();
    if (MapPositionToOrientation(lrFrame.mPlayerTransform, lSpyModified.mPointOnA,
                                 lSpyModified.mNormal, lSpyModified.mEntityIdA,
                                 lSpyModified.mEntityIdB, lMgr, meOrientation))
    {
        mScrapeInfo = ScrapeInfo(lSpyModified, meOrientation, lfTimeStamp, maParameter[0].x);
    }
    else
    {
        InputContactSpy lNewSpy = lSpyModified;
        SwapEntityIds(lNewSpy);
        maEntityID[0] = lNewSpy.mEntityIdA;
        maEntityID[1] = lNewSpy.mEntityIdB;
        MapPositionToOrientation(lrFrame.mPlayerTransform, lNewSpy.mPointOnA, lNewSpy.mNormal,
                                 lNewSpy.mEntityIdA, lNewSpy.mEntityIdB, lMgr, meOrientation);
        mScrapeInfo = ScrapeInfo(lNewSpy, meOrientation, lfTimeStamp, maParameter[0].x);
    }

    // The EeMaterialType is a 32-bit enum; `extsw` widens it (0x826D3BA4 / 0x826D3BE8).
    maMaterial[0] = static_cast<u64>(static_cast<s64>(MapEntityIdToMaterial(
        maEntityID[0], lInput.GetVehicleInterface()->GetPlayerActiveRaceCarIndex(), lInput)));
    maMaterial[1] = static_cast<u64>(static_cast<s64>(MapEntityIdToMaterial(
        maEntityID[1], lInput.GetVehicleInterface()->GetPlayerActiveRaceCarIndex(), lInput)));

    const Vector3 lSpyToCamera = { mPosition.x - lCamera.mTransform.Pos().x,
                                   mPosition.y - lCamera.mTransform.Pos().y,
                                   mPosition.z - lCamera.mTransform.Pos().z, 0.0f };
    const f32 lfDistanceSquared = Dot3(lSpyToCamera, lSpyToCamera);
    maParameter[1] = Splat(lfDistanceSquared);
    const f32 lfInverseDistance = 1.0f / std::sqrt(lfDistanceSquared);
    const Vector3 lDirection = { lSpyToCamera.x * lfInverseDistance,
                                 lSpyToCamera.y * lfInverseDistance,
                                 lSpyToCamera.z * lfInverseDistance, 0.0f };
    maParameter[2] = Splat(Dot3(lDirection, lCamera.mTransform.At()));

    if (lrFrame.meImpactTime.GetCurrent() != AttribSys::Enums::eImpactTime::False)
    {
        const ScrapeInfo* lpScrape = lMgr.FindInScrapeHistory(mScrapeInfo);   // bl 0x826889E0
        if (lpScrape)
        {
            const f32 lfAge = lfTimeStamp - lpScrape->mfTimeStamp;
            if (lfAge < 5.0f)
                mbCull = true;
        }
    }
}

// ---------------------------------------------------------------------------
// InputCollision::InputCollision(const CameraInfo&, CollisionStateManager&, const InputPropSpy&,
//                                const LogicInputBuffer&, f32, f32)   sub_826E8B20 (DWARF h:414)
//
// The prop contacts (ImportContactSpies 0x826EB490):
//   0x826E8B64..0x826E8BAC  mScrapeInfo.mbValid = 0, mbCull = 0, mfPriorityAddition = 0.0,
//                           meAction = Collision, mePipeline = E_PROP, mPosition = the point on A,
//                           maEntityID = (A, B)
//   0x826E8BB0..0x826E8BC0  MapPositionToOrientation from B's side at B's contact point (the result
//                           is not tested)
//   0x826E8BC4..0x826E8C2C  maMaterial[1] = `extsw` MapEntityIdToMaterial(B)
//   0x826E8C10..0x826E8D04  impulse, distance and facing exactly as the regular builder
//   0x826E8D08..0x826E8D70  the prop's own material from MapPropTypeToMaterial into a zeroed
//                           local; mbCull = it has none; a prop that began moving this frame adds
//                           priority 1.0; maMaterial[0] = that local (0 when unmapped)
//   0x826E8D74..0x826E8DFC  B a prop too (owner byte 3): this collision becomes A against the
//                           world (maEntityID[1] = 0, maMaterial[1] = World) and B gets its own
//                           collision against the world -- a copy of the spy with A := B, B := 0 and
//                           the two points exchanged -- added straight to the manager's inputs
// ---------------------------------------------------------------------------
InputCollision::InputCollision(const CameraInfo& lCamera, CollisionStateManager& lMgr,
                               const InputPropSpy& lSpy, const LogicInputBuffer& lInput,
                               f32 lfTimeStamp, f32 lfTimeStep)
    : maMaterial{0, 0}
    , maEntityID{{0}, {0}}
    , mfPriorityAddition(0.0f)
    , meAction(AttribSys::Enums::eAction::Collision)
    , meOrientation(AttribSys::Enums::eOrientation::Front)
    , mePipeline(E_PROP)
    , mbCull(false)
{
    mScrapeInfo.mbValid = false;
    mPosition = lSpy.mPointOnA;
    maEntityID[0] = lSpy.mEntityIdA;
    maEntityID[1] = lSpy.mEntityIdB;

    MapPositionToOrientation(lMgr.GetFrameInformation().mPlayerTransform, lSpy.mPointOnB,
                             lSpy.mNormal, lSpy.mEntityIdB, lSpy.mEntityIdA, lMgr, meOrientation);

    maMaterial[1] = static_cast<u64>(static_cast<s64>(MapEntityIdToMaterial(
        lSpy.mEntityIdB, lInput.GetVehicleInterface()->GetPlayerActiveRaceCarIndex(), lInput)));

    maParameter[0] = Splat(NormalisedImpulse(lSpy.mNormalStress, lfTimeStep));
    const Vector3 lSpyToCamera = { mPosition.x - lCamera.mTransform.Pos().x,
                                   mPosition.y - lCamera.mTransform.Pos().y,
                                   mPosition.z - lCamera.mTransform.Pos().z, 0.0f };
    const f32 lfDistanceSquared = Dot3(lSpyToCamera, lSpyToCamera);
    maParameter[1] = Splat(lfDistanceSquared);
    const f32 lfInverseDistance = 1.0f / std::sqrt(lfDistanceSquared);
    const Vector3 lDirection = { lSpyToCamera.x * lfInverseDistance,
                                 lSpyToCamera.y * lfInverseDistance,
                                 lSpyToCamera.z * lfInverseDistance, 0.0f };
    maParameter[2] = Splat(Dot3(lDirection, lCamera.mTransform.At()));

    u64 luMaterial = 0;
    mbCull = !lMgr.MapPropTypeToMaterial(lSpy.muType, luMaterial);
    if (lSpy.muBeganMoving == 1u)
        mfPriorityAddition = 1.0f;
    maMaterial[0] = luMaterial;

    if (GetEntityOwner(lSpy.mEntityIdB) == KU_ENTITY_OWNER_PROP)
    {
        maEntityID[1].muValue = 0;
        maMaterial[1] = AttribSys::Enums::eMaterialType::World;

        InputPropSpy lNewSpy = lSpy;
        lNewSpy.mEntityIdA = lSpy.mEntityIdB;
        lNewSpy.mEntityIdB.muValue = 0;
        lNewSpy.mPointOnA = lSpy.mPointOnB;
        lNewSpy.mPointOnB = lSpy.mPointOnA;
        InputCollision lNewInputCollision(lCamera, lMgr, lNewSpy, lInput, lfTimeStamp, lfTimeStep);
        lMgr.AddInputCollision(lNewInputCollision);
    }
}

// ---------------------------------------------------------------------------
// InputCollision::InputCollision(const CameraInfo&, CollisionStateManager&, const DiscardedContact&,
//                                const LogicInputBuffer&, f32, f32)   sub_826BDAE8 (DWARF h:424)
//
// The discarded contacts (ImportContactSpies<EventQueue<DiscardedContact,20>> 0x826DD1C0):
//   0x826BDAF4..0x826BDB58  mScrapeInfo.mbValid = 0 (+0x29), mePipeline = E_REGULAR (+0x94),
//                           mfPriorityAddition = 0.0 (flt_82001CC0, +0x88), meAction = Collision
//                           (+0x8C), mbCull = 0 (+0x98), mPosition = the point on A (+0x60),
//                           maEntityID = (A, B) (+0x80 / +0x84)
//   0x826BDB5C..0x826BDB6C  meOrientation from MapPositionToOrientation (A's side, at A's point, with
//                           the contact normal; the result is not tested)
//   0x826BDB70..0x826BDC28  maMaterial[0] / [1] = `extsw` MapEntityIdToMaterial(A) / (B) with the
//                           vehicle interface's player index (the h:980 assert before each); NO
//                           0x2000000000 bit on [1] -- Hex-Rays prints one, the instructions are
//                           `extsw r9,r3 ; std r9,0x78(r31)` (0x826BDC0C / 0x826BDC28)
//   0x826BDC2C..0x826BDC64  maParameter[0] = splat(the record's closing velocity, +8) -- as it comes,
//                           no normal stress and no 1 / (K * dt) scale
//   0x826BDC10..0x826BDCA8  maParameter[1] = |position - camera|^2, maParameter[2] =
//                           normalize(position - camera) . the camera's At row, as the regular builder
// Unlike the regular builder: no scrape entry, no race-car ordering, no pair reversal, no SloMoCrash
// culling -- and the stamp and the step (f1 / f2) are never read.
// ---------------------------------------------------------------------------
InputCollision::InputCollision(const CameraInfo& lCamera, CollisionStateManager& lMgr,
                               const BrnPhysics::ContactSpy::DiscardedContact& lSpy,
                               const LogicInputBuffer& lInput, f32 /*lfTimeStamp*/, f32 /*lfTimeStep*/)
    : maMaterial{0, 0}
    , maEntityID{{0}, {0}}
    , mfPriorityAddition(0.0f)
    , meAction(AttribSys::Enums::eAction::Collision)
    , meOrientation(AttribSys::Enums::eOrientation::Front)
    , mePipeline(E_REGULAR)
    , mbCull(false)
{
    mScrapeInfo.mbValid = false;
    mPosition = lSpy.mPointOnA;
    maEntityID[0] = lSpy.mEntityIdA;
    maEntityID[1] = lSpy.mEntityIdB;

    MapPositionToOrientation(lMgr.GetFrameInformation().mPlayerTransform, lSpy.mPointOnA,
                             lSpy.mNormal, lSpy.mEntityIdA, lSpy.mEntityIdB, lMgr, meOrientation);

    maMaterial[0] = static_cast<u64>(static_cast<s64>(MapEntityIdToMaterial(
        lSpy.mEntityIdA, lInput.GetVehicleInterface()->GetPlayerActiveRaceCarIndex(), lInput)));
    maMaterial[1] = static_cast<u64>(static_cast<s64>(MapEntityIdToMaterial(
        lSpy.mEntityIdB, lInput.GetVehicleInterface()->GetPlayerActiveRaceCarIndex(), lInput)));

    maParameter[0] = Splat(lSpy.mfClosingVelocity);
    const Vector3 lSpyToCamera = { mPosition.x - lCamera.mTransform.Pos().x,
                                   mPosition.y - lCamera.mTransform.Pos().y,
                                   mPosition.z - lCamera.mTransform.Pos().z, 0.0f };
    const f32 lfDistanceSquared = Dot3(lSpyToCamera, lSpyToCamera);
    maParameter[1] = Splat(lfDistanceSquared);
    const f32 lfInverseDistance = 1.0f / std::sqrt(lfDistanceSquared);
    const Vector3 lDirection = { lSpyToCamera.x * lfInverseDistance,
                                 lSpyToCamera.y * lfInverseDistance,
                                 lSpyToCamera.z * lfInverseDistance, 0.0f };
    maParameter[2] = Splat(Dot3(lDirection, lCamera.mTransform.At()));
}

// =================================================================================================
// THE SCRAPE LEGS (FX-CRASHSND2 item 2). A contact that goes on touching the same thing on the same
// face -- the same entity pair and orientation (ScrapeInfo::operator== @0x826821F0) -- is a scrape:
// UpdateScrapes eats its repeated impacts, hands it to a collision state with the SCRAPE lifetime
// (whose ScrapeEffect runs the AEMS scrape granulator) and keeps it in the 16-entry history while it
// stays fresh. Before this the history was never written, so no scrape ever sounded and every frame
// of a grind was a fresh impact.
// =================================================================================================

// KF_SCRAPE_IDLE_TIME (DWARF cpp:77) -- unk_82FFBF40 = splat(0.2f), written by the CRT thunk
// 0x82C63250 from 0x82004744 (0x3E4CCCCD): a history scrape not refreshed for this long is dropped.
VecFloat KF_SCRAPE_IDLE_TIME = { 0.2f, 0.2f, 0.2f, 0.2f };

// KF_CULLING_DISTANCE_SQUARED (DWARF cpp:86) -- unk_82FFBF50 = splat(2500.0f), CRT 0x82C632E0 from
// 0x8200D510 (0x451C4000): only scrapes within 50 m of the player are tracked. Named by the CRT
// order: cpp:85 KF_EARLY_CULL_DISTANCE_SQUARED is the thunk before it (0x82C632B8, 4900, the one
// AddInputCollision reads at 0x830085A0).
const VecFloat KF_CULLING_DISTANCE_SQUARED = { 2500.0f, 2500.0f, 2500.0f, 2500.0f };

// ---------------------------------------------------------------------------
// CollisionStateManager::FindOldestScrapeInHistory()  sub_82688A58  (DWARF cpp:2881)
//
// The first free history slot (`lbz 0x29 ; beq`), else the one with the smallest time stamp
// (`fcmpu ; bge` -- a strict less-than from 1e10, flt_82011E3C). Asserts it found one (cpp:2992).
// ---------------------------------------------------------------------------
BrnSound::Logic::Collision::ScrapeInfo* CollisionStateManager::FindOldestScrapeInHistory()
{
    s32 liOldestIndex = -1;
    f32 lfOldestTimeStamp = 1.0e10f;
    for (s32 liIndex = 0; liIndex < E_MAX_SCRAPE_HISTORY; ++liIndex)
    {
        if (!maScrapeHistory[liIndex].mbValid)
            return &maScrapeHistory[liIndex];
        if (maScrapeHistory[liIndex].mfTimeStamp < lfOldestTimeStamp)
        {
            lfOldestTimeStamp = maScrapeHistory[liIndex].mfTimeStamp;
            liOldestIndex = liIndex;
        }
    }
    CGS_ASSERT(liOldestIndex >= 0 && liOldestIndex < E_MAX_SCRAPE_HISTORY,
               "liOldestIndex >= 0 && liOldestIndex < E_MAX_SCRAPE_HISTORY");
    return &maScrapeHistory[liOldestIndex];
}

// ---------------------------------------------------------------------------
// CollisionStateManager::UniqueScrape(const ScrapeInfo&, const InputCollision* const*, u32) const
//   @ 0x82688B20  (DWARF cpp:2975)
//
// False as soon as one listed collision's scrape is the same scrape (`bl operator==` with the
// listed collision as `this`), else true.
// ---------------------------------------------------------------------------
bool CollisionStateManager::UniqueScrape(const BrnSound::Logic::Collision::ScrapeInfo& lScrapeInfo,
                                         const InputCollision* const* lapCollisions,
                                         u32 lu32Count) const
{
    for (u32 lu32Index = 0; lu32Index < lu32Count; ++lu32Index)
    {
        if (lapCollisions[lu32Index]->mScrapeInfo == lScrapeInfo)
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// CollisionStateManager::FindEntity(const EntityId&, GenericEntity&) const  @ 0x826A0398
//   (DWARF cpp:3000)
//
// Reads one side of a scrape from the sound input (asserts mpBrnLogicInputBuffer, h:432, and
// lpInputBuffer, cpp:3100). Only the fields of the entity's own kind are written -- the caller's
// entity keeps whatever an earlier call left in the others:
//   owner 0 (world)     mbWorld = true                                            0x826A0580
//   owner 1 (race car)  false unless IsRaceCarActive and GetRaceCarState; else mbCrashing
//                       (+0x44A), mPosition = the transform's position (+0x220), mVelocity =
//                       mLinearVelocity (+0x330), mbPlayer = IsRaceCarPlayer      0x826A04EC..0x826A0568
//   owner 2 (traffic)   the traffic sound entity (GetTrafficOutputInterface @0x82694DD8,
//                       GetTrafficEntityIndex @0x82681EC8); none -> false; else mbCrashing =
//                       mbIsCrashed (+0x4D), mPosition = its position (+0x30), mVelocity = its At
//                       row (+0x20) scaled by mfSpeed (+0x44, vmulfp128 by the splat)  0x826A0494..0x826A04E0
//   owner 3+            false                                                     0x826A0424
// [NOT IN THIS TREE] while the replay serialiser plays back (sound logic module +0x13600, modes
// 4..6) the console reads traffic from the recording instead (SoundSerialiser::
// GetTrafficEntityByTrafficIndex @0x826823E0). This tree's SoundLogicModule holds no
// SoundSerialiser and nothing drives a serialiser's mode (BrnSoundLogicModule.cpp case 218), so
// only the live traffic interface is read.
// ---------------------------------------------------------------------------
bool CollisionStateManager::FindEntity(const EntityId& lEntityId, GenericEntity& lEntity) const
{
    const LogicInputBuffer* lpInputBuffer =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule())->GetBrnInputStructure();
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer");

    const u32 luOwner = GetEntityOwner(lEntityId);
    if (luOwner == KU_ENTITY_OWNER_WORLD)
    {
        lEntity.mbWorld = true;
        return true;
    }
    if (luOwner == KU_ENTITY_OWNER_RACE_CAR)
    {
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& lrVehicles =
            *lpInputBuffer->GetVehicleInterface();
        const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>(GetEntityIndex(lEntityId));
        if (!lrVehicles.IsRaceCarActive(leIndex))
            return false;
        const BrnPhysics::Vehicle::RaceCarState* lpState = lrVehicles.GetRaceCarState(leIndex);
        if (!lpState)
            return false;
        lEntity.mbCrashing = lpState->mbCrashing;
        lEntity.mPosition = lpState->mTransform.Pos();
        lEntity.mVelocity = lpState->mLinearVelocity;
        // 0x826A0554..0x826A0568: `rlwinm r4, id, 22, 18, 31 ; bl IsRaceCarPlayer` (0x826A055C ->
        // 0x82681DF0: the two index asserts, then bit 1 -- E_RACE_CAR_OUTPUT_FLAG_PLAYER, raised for the
        // PLAYER-type car -- of maxRaceCarFlags[idx]) ; `stb r3, 0x21(entity)`. [FX-AIBUZZ 2026-09-24:
        // the accessor is bodied now; this was the stand-in `GetPlayerActiveRaceCarIndex() == leIndex`,
        // which reads a different member and asserts/logs while the player index is unset]
        lEntity.mbPlayer = lrVehicles.IsRaceCarPlayer(leIndex);
        return true;
    }
    if (luOwner >= KU_ENTITY_OWNER_PROP)
        return false;

    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* lpTraffic =
        lpInputBuffer->GetTrafficOutputInterface().GetTrafficEntityIndex(
            static_cast<u16>(GetEntityIndex(lEntityId)));
    if (!lpTraffic)
        return false;
    lEntity.mbCrashing = lpTraffic->mbIsCrashed;
    lEntity.mPosition = lpTraffic->mLocalTransform.Pos();
    const Vector3& lrAt = lpTraffic->mLocalTransform.At();
    const f32 lfSpeed = lpTraffic->mfSpeed;
    lEntity.mVelocity.x = lrAt.x * lfSpeed;
    lEntity.mVelocity.y = lrAt.y * lfSpeed;
    lEntity.mVelocity.z = lrAt.z * lfSpeed;
    lEntity.mVelocity.w = lrAt.w * lfSpeed;
    return true;
}

// ---------------------------------------------------------------------------
// CollisionStateManager::UpdateScrapeHistory(const FrameInformation&)  @ 0x826BEB98
//   (DWARF cpp:2916; the frame is not read)
//
// Two GenericEntity locals, built ONCE before the loop, and the last pair looked up (both -1): a
// slot whose side is the same entity as the previous slot's reuses what the previous lookup left.
// For each valid slot:
//   not (KF_SCRAPE_IDLE_TIME > now - stamp)  -> dropped (all-lanes vcmpgtfp.; a NaN age drops)
//   A differs from the last A and FindEntity(A) fails, or the same for B  -> dropped
//   else the pair is remembered, mRelativeVelocity = B's velocity - A's (vsubfp, 0x826BED54;
//   asserted not NaN, cpp:3040) and mbCrashing = a player's car in the pair is crashing
//   (0x826BED98..0x826BEDD8).
// ---------------------------------------------------------------------------
void CollisionStateManager::UpdateScrapeHistory(const BrnSound::Logic::FrameInformation& /*lrFrame*/)
{
    GenericEntity lEntityA;
    GenericEntity lEntityB;
    u32 lu32LastEntityA = 0xFFFFFFFFu;
    u32 lu32LastEntityB = 0xFFFFFFFFu;
    const f32 lfTime = mfCurrentTime;

    for (u32 lu32Index = 0; lu32Index < E_MAX_SCRAPE_HISTORY; ++lu32Index)
    {
        BrnSound::Logic::Collision::ScrapeInfo& lrScrape = maScrapeHistory[lu32Index];
        if (!lrScrape.mbValid)
            continue;

        const f32 lfAge = lfTime - lrScrape.mfTimeStamp;
        if (!(KF_SCRAPE_IDLE_TIME.x > lfAge) ||
            (lu32LastEntityA != lrScrape.mEntityIdA.muValue && !FindEntity(lrScrape.mEntityIdA, lEntityA)) ||
            (lu32LastEntityB != lrScrape.mEntityIdB.muValue && !FindEntity(lrScrape.mEntityIdB, lEntityB)))
        {
            lrScrape.mbValid = false;
            continue;
        }
        lu32LastEntityA = lrScrape.mEntityIdA.muValue;
        lu32LastEntityB = lrScrape.mEntityIdB.muValue;

        lrScrape.mRelativeVelocity.x = lEntityB.mVelocity.x - lEntityA.mVelocity.x;
        lrScrape.mRelativeVelocity.y = lEntityB.mVelocity.y - lEntityA.mVelocity.y;
        lrScrape.mRelativeVelocity.z = lEntityB.mVelocity.z - lEntityA.mVelocity.z;
        lrScrape.mRelativeVelocity.w = lEntityB.mVelocity.w - lEntityA.mVelocity.w;
        CGS_ASSERT(lrScrape.mRelativeVelocity.x == lrScrape.mRelativeVelocity.x &&
                   lrScrape.mRelativeVelocity.y == lrScrape.mRelativeVelocity.y &&
                   lrScrape.mRelativeVelocity.z == lrScrape.mRelativeVelocity.z,
                   "maScrapeHistory[ i ].mRelativeVelocity == maScrapeHistory[ i ].mRelativeVelocity");

        lrScrape.mbCrashing = (lEntityA.mbPlayer && lEntityA.mbCrashing) ||
                              (lEntityB.mbPlayer && lEntityB.mbCrashing);
    }
}

// ---------------------------------------------------------------------------
// CollisionStateManager::UpdateScrapes(const FrameInformation&)  @ 0x826D3F50  (DWARF cpp:2678)
//
// Called by UpdateResolver after every contact is imported, before CullInputCollisions
// (0x826F9720). After UpdateScrapeHistory, each input collision that carries a scrape and lies
// within KF_CULLING_DISTANCE_SQUARED of the player (the frame's player position, +0x30; a NaN
// distance counts as near) is looked up in the history:
//   found      -> if it is younger than 0.1 s (flt_820ABB14) and not listed yet, list it as a
//                 CONTINUING scrape and give it the history's relative velocity; the history takes
//                 its stamp and intensity (ScrapeInfo::UpdateHistory) and it takes the history's
//                 mbCrashing; outside a fatality (meFatality == E_FATAL_OFF) the collision is eaten
//                 (mbCull -- "Scraping culling. Collision is eaten.", 0x826D40F0..0x826D4100)
//   not found  -> list it as a NEW scrape (once per scrape)
// Then:
//   continuing  -> every attached collision state whose own scrape is this scrape takes the
//                  SCRAPE lifetime and the new scrape (0x826D41B8..0x826D42B0: each state and each
//                  listed scrape pairs off at most once); a scrape no state took gets a free state
//                  (the BASE StateManager::GetFreeState @0x8268D7D0, a direct call -- no priority
//                  steal) attached to an OutputCollision holding just its position and scrape,
//                  with the SCRAPE lifetime (0x826D42C8..0x826D4340); no free state ends this leg
//   new         -> recorded over the oldest history slot when FindEntity knows both sides
//                  (0x826D4380..0x826D44B0; asserts the slot, cpp:2930)
// [NOT IN THIS TREE] the replay recorder: the console keeps a BrnReplays::SoundSerialiser in the
// sound logic module (+0x13600) and, while it records (modes 1..3), zeroes the frame's scrape count
// (0x826D3F90..0x826D3FC8) and mirrors every tracked scrape into it while it holds fewer than 4
// (AddScrape @0x826959B8, 0x826D4160..0x826D4194). This tree's SoundLogicModule holds no
// SoundSerialiser and nothing drives a serialiser's mode (BrnSoundLogicModule.cpp case 218).
// The eaten-collision spew (dword_82FFB91C >= 3 -> InputCollision::DebugPrint) is a developer
// print on a zero .bss level; left out.
// ---------------------------------------------------------------------------
void CollisionStateManager::UpdateScrapes(const BrnSound::Logic::FrameInformation& lrFrame)
{
    UpdateScrapeHistory(lrFrame);

    const f32 lfTime = mfCurrentTime;
    const InputCollision* lapContinuing[64];
    const InputCollision* lapNew[64];
    u32 lu32Continuing = 0;
    u32 lu32New = 0;

    for (u32 lu32Index = 0; lu32Index < mu32InputCollisionCount; ++lu32Index)
    {
        InputCollision& lrInput = maInputCollision[lu32Index];
        if (!lrInput.mScrapeInfo.mbValid)
            continue;

        const Vector3& lrPlayer = lrFrame.mPlayerTransform.Pos();
        const f32 lfX = lrPlayer.x - lrInput.mPosition.x;
        const f32 lfY = lrPlayer.y - lrInput.mPosition.y;
        const f32 lfZ = lrPlayer.z - lrInput.mPosition.z;
        if (lfX * lfX + lfY * lfY + lfZ * lfZ >= KF_CULLING_DISTANCE_SQUARED.x)
            continue;

        BrnSound::Logic::Collision::ScrapeInfo* lpScrape = FindInScrapeHistory(lrInput.mScrapeInfo);
        if (lpScrape)
        {
            if (lfTime - lpScrape->mfTimeStamp < 0.1f &&
                UniqueScrape(lrInput.mScrapeInfo, lapContinuing, lu32Continuing))
            {
                lapContinuing[lu32Continuing++] = &lrInput;
                lrInput.mScrapeInfo.mRelativeVelocity = lpScrape->mRelativeVelocity;
            }
            lpScrape->UpdateHistory(lrInput.mScrapeInfo);
            lrInput.mScrapeInfo.mbCrashing = lpScrape->mbCrashing;
            if (mFrameInformation.meFatality.GetCurrent() == E_FATAL_OFF)
            {
                lrInput.mbCull = true;
                // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): a repeated impact eaten.
                if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
                {
                    static u32 suEatenPrintCount = 0;
                    if (suEatenPrintCount++ < 16u)
                    {
                        char lacLine[160];
                        std::snprintf(lacLine, sizeof(lacLine),
                                      "[collision-audio] scrape eaten A=%u:%u B=%u:%u orient=%d\n",
                                      GetEntityOwner(lrInput.mScrapeInfo.mEntityIdA),
                                      GetEntityIndex(lrInput.mScrapeInfo.mEntityIdA),
                                      GetEntityOwner(lrInput.mScrapeInfo.mEntityIdB),
                                      GetEntityIndex(lrInput.mScrapeInfo.mEntityIdB),
                                      static_cast<s32>(lrInput.mScrapeInfo.meOrientation));
                        *CgsDev::Log::gpDebugPrint << lacLine;
                    }
                }
            }
        }
        else if (UniqueScrape(lrInput.mScrapeInfo, lapNew, lu32New))
        {
            lapNew[lu32New++] = &lrInput;
        }
    }

    if (lu32Continuing != 0u)
    {
        CollisionState* lapStates[32];
        u32 lu32States = 0;
        for (CgsSound::Logic::State* lpState = GetHeadState(); lpState; lpState = lpState->GetNextState())
        {
            if (lpState->IsAttached())
                lapStates[lu32States++] = static_cast<CollisionState*>(lpState);
        }

        for (u32 lu32State = 0; lu32State < lu32States; ++lu32State)
        {
            CollisionState* lpState = lapStates[lu32State];
            for (u32 lu32Scrape = 0; lu32Scrape < lu32Continuing; ++lu32Scrape)
            {
                if (!lpState || !lapContinuing[lu32Scrape])
                    continue;
                BrnSound::Logic::Collision::ScrapeInfo& lrStateScrape =
                    lpState->GetOutputCollision().mScrapeInfo;
                if (!lrStateScrape.mbValid || !(lrStateScrape == lapContinuing[lu32Scrape]->mScrapeInfo))
                    continue;
                lpState->SetLifetime(CollisionState::E_SCRAPE);
                lrStateScrape = lapContinuing[lu32Scrape]->mScrapeInfo;
                // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): a state keeps a scrape.
                if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
                {
                    static u32 suRefreshPrintCount = 0;
                    if (suRefreshPrintCount++ < 16u)
                    {
                        char lacLine[160];
                        std::snprintf(lacLine, sizeof(lacLine),
                                      "[collision-audio] scrape refresh A=%u:%u B=%u:%u orient=%d intensity=%.3f\n",
                                      GetEntityOwner(lrStateScrape.mEntityIdA), GetEntityIndex(lrStateScrape.mEntityIdA),
                                      GetEntityOwner(lrStateScrape.mEntityIdB), GetEntityIndex(lrStateScrape.mEntityIdB),
                                      static_cast<s32>(lrStateScrape.meOrientation), lrStateScrape.mfIntensity);
                        *CgsDev::Log::gpDebugPrint << lacLine;
                    }
                }
                lpState = nullptr;
                lapContinuing[lu32Scrape] = nullptr;
            }
            lapStates[lu32State] = lpState;
        }
    }

    for (u32 lu32Scrape = 0; lu32Scrape < lu32Continuing; ++lu32Scrape)
    {
        const InputCollision* lpInput = lapContinuing[lu32Scrape];
        if (!lpInput)
            continue;
        OutputCollision lOutput;
        lOutput.mPosition = lpInput->mPosition;
        lOutput.mScrapeInfo = lpInput->mScrapeInfo;
        CollisionState* lpState =
            static_cast<CollisionState*>(CgsSound::Logic::StateManager::GetFreeState(nullptr));
        if (!lpState)
            break;
        lpState->Attach(&lOutput);
        lpState->SetLifetime(CollisionState::E_SCRAPE);
        // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): a scrape takes a free state.
        if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
        {
            static u32 suStartPrintCount = 0;
            if (suStartPrintCount++ < 16u)
            {
                char lacLine[160];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[collision-audio] scrape start A=%u:%u B=%u:%u orient=%d intensity=%.3f\n",
                              GetEntityOwner(lOutput.mScrapeInfo.mEntityIdA), GetEntityIndex(lOutput.mScrapeInfo.mEntityIdA),
                              GetEntityOwner(lOutput.mScrapeInfo.mEntityIdB), GetEntityIndex(lOutput.mScrapeInfo.mEntityIdB),
                              static_cast<s32>(lOutput.mScrapeInfo.meOrientation), lOutput.mScrapeInfo.mfIntensity);
                *CgsDev::Log::gpDebugPrint << lacLine;
            }
        }
    }

    for (u32 lu32Scrape = 0; lu32Scrape < lu32New; ++lu32Scrape)
    {
        GenericEntity lEntityA;
        GenericEntity lEntityB;
        const InputCollision* lpInput = lapNew[lu32Scrape];
        if (FindEntity(lpInput->mScrapeInfo.mEntityIdA, lEntityA) &&
            FindEntity(lpInput->mScrapeInfo.mEntityIdB, lEntityB))
        {
            BrnSound::Logic::Collision::ScrapeInfo* lpScrape = FindOldestScrapeInHistory();
            CGS_ASSERT(lpScrape != nullptr, "lpScrape");
            *lpScrape = lpInput->mScrapeInfo;
            // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): a scrape enters the history.
            if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
            {
                static u32 suHistoryPrintCount = 0;
                if (suHistoryPrintCount++ < 16u)
                {
                    char lacLine[160];
                    std::snprintf(lacLine, sizeof(lacLine),
                                  "[collision-audio] scrape history A=%u:%u B=%u:%u orient=%d stamp=%.3f\n",
                                  GetEntityOwner(lpScrape->mEntityIdA), GetEntityIndex(lpScrape->mEntityIdA),
                                  GetEntityOwner(lpScrape->mEntityIdB), GetEntityIndex(lpScrape->mEntityIdB),
                                  static_cast<s32>(lpScrape->meOrientation), lpScrape->mfTimeStamp);
                    *CgsDev::Log::gpDebugPrint << lacLine;
                }
            }
        }
    }
}

// =================================================================================================
// THE DEFORMATION LEGS (FX-CRASHSND2 item 3). What the deformation system and the car-part contact
// spies report -- a glass pane smashing, a hinged door or bonnet opening / closing / swinging, a joint
// breaking, a part coming off, a detached part or wheel hitting something -- becomes an InputCollision
// of the regular pipeline (UpdateResolver 0x826F940C..0x826F94B0). Before this none of it reached the
// collision sound: the sound input carried only an opaque 4-byte copy of the deformation output.
// =================================================================================================

// KF_MAX_IMPULSE (DWARF cpp:62) -- unk_82FFBF60 = splat(3.0f), written by the CRT thunk 0x82C631E0 from
// 0x82004270 (0x40400000): the impulse lane of a glass, broken-joint and detached-part collision.
// Named by the CRT order of the four VecFloats cpp:60..63 (0x82C63190 2.0, 0x82C631B8 10.0,
// 0x82C631E0 3.0, 0x82C63208 0.1).
const VecFloat KF_MAX_IMPULSE = { 3.0f, 3.0f, 3.0f, 3.0f };

// KF_HINGING_VELOCITY_MIN_TOLERANCE (DWARF cpp:66) -- flt_82F2CEF0 = 6.0f; KF_HINGING_VELOCITY_DELTA_
// TOLERANCE (cpp:67) -- flt_82F2CEF4 = 0.1f (.data, in declaration order ahead of KF_FASTEST_COLLISION
// 0x82F2CEF8 and KF_BIGGEST_THING_MASS 0x82F2CEFC): a swinging part sounds when its hinge velocity
// changed by more than the delta while one of the two velocities exceeds the minimum.
f32 KF_HINGING_VELOCITY_MIN_TOLERANCE = 6.0f;
f32 KF_HINGING_VELOCITY_DELTA_TOLERANCE = 0.1f;

namespace
{
// UpdateHingingBodyParts's open / closed test: an orientation within 0.08 (flt_82F2FD30 = 0x3DA3D70A)
// of 1 is open, of 0 closed.
const f32 KF_HINGE_OPEN_CLOSED_EPSILON = 0.08f;

// The regular builders' camera lanes (maParameter[1] = the distance squared to the camera, [2] = how
// much the camera faces the point). The glass and hinge builders dot the RAW offset with the camera's
// At row (0x826BE52C..0x826BE540 / 0x826BDF60's tail -- no normalisation); the contact-spy builders
// normalise it first (the rsqrte refinement, 0x826BE204..0x826BE240).
VecFloat CameraDistanceSquared(const Vector3& lrPosition, const CameraInfo& lrCamera)
{
    const Vector3 lOffset = { lrPosition.x - lrCamera.mTransform.Pos().x,
                              lrPosition.y - lrCamera.mTransform.Pos().y,
                              lrPosition.z - lrCamera.mTransform.Pos().z, 0.0f };
    return Splat(Dot3(lOffset, lOffset));
}

VecFloat CameraFacingRaw(const Vector3& lrPosition, const CameraInfo& lrCamera)
{
    const Vector3 lOffset = { lrPosition.x - lrCamera.mTransform.Pos().x,
                              lrPosition.y - lrCamera.mTransform.Pos().y,
                              lrPosition.z - lrCamera.mTransform.Pos().z, 0.0f };
    return Splat(Dot3(lOffset, lrCamera.mTransform.At()));
}

VecFloat CameraFacingNormalised(const Vector3& lrPosition, const CameraInfo& lrCamera)
{
    const Vector3 lOffset = { lrPosition.x - lrCamera.mTransform.Pos().x,
                              lrPosition.y - lrCamera.mTransform.Pos().y,
                              lrPosition.z - lrCamera.mTransform.Pos().z, 0.0f };
    const f32 lfInverseDistance = 1.0f / std::sqrt(Dot3(lOffset, lOffset));
    const Vector3 lDirection = { lOffset.x * lfInverseDistance, lOffset.y * lfInverseDistance,
                                 lOffset.z * lfInverseDistance, 0.0f };
    return Splat(Dot3(lDirection, lrCamera.mTransform.At()));
}

// The sound input the builders read the player index from: the collision manager's module's
// (`lwz 0x2C(mgr) ; lwz 0x4C94` -- SoundLogicModule::GetBrnInputStructure, asserted h:432).
const LogicInputBuffer& ModuleInput(const CollisionStateManager& lrMgr)
{
    return *static_cast<BrnSound::Module::SoundLogicModule*>(lrMgr.GetLogicModule())->GetBrnInputStructure();
}
}

// ---------------------------------------------------------------------------
// MapBodyPartEnumToMateral(EBodyParts)  @ 0x82688CF8  (DWARF cpp:3677, the console's spelling)
//
// The switch table of the image, by EBodyParts value (names: DWARF SharedClasses/Physics/
// BrnPhysicsPartTypes.h; this tree's EBodyParts carries no enumerators):
// ---------------------------------------------------------------------------
EeMaterialType MapBodyPartEnumToMateral(BrnPhysics::Deformation::EBodyParts leBodyPart)
{
    using namespace AttribSys::Enums::eMaterialType;
    switch (static_cast<s32>(leBodyPart))
    {
    case 0:     // eBody_Roof_PLAYERONLY
    case 3:     // eBody_Bonnet
    case 4:     // eBody_Boot
    case 8:     // eBody_DoorLeft
    case 10:    // eBody_DoorRight
    case 12:    // eBody_DoorRearLeft
    case 13:    // eBody_DoorRearRight
    case 14:    // eBody_DoorBackLeft
    case 15:    // eBody_DoorBackRight
        return BodyPartLarge;
    case 1:     // eBody_BumperFront
    case 2:     // eBody_BumperRear
    case 5:     // eBody_Spoiler
    case 6:     // eBody_GrillFront
    case 7:     // eBody_GrillRear
    case 24:    // eBody_SkirtLeft
    case 25:    // eBody_SkirtRight
    case 26:    // eBody_WingFrontLeft
    case 27:    // eBody_WingFrontRight
    case 28:    // eBody_WingRearRight
    case 29:    // eBody_WingRearLeft
        return BodyPartSmall;
    case 9:     // eBody_DoorLeftMirror
    case 11:    // eBody_DoorRightMirror
    case 103:   // eInterior_InteriorMirror
        return Mirrors;
    case 16:    // eBody_GlassDoorFrontLeft
    case 17:    // eBody_GlassDoorFrontRight
    case 18:    // eBody_GlassDoorRearLeft
    case 19:    // eBody_GlassDoorRearRight
        return GlassSmall;
    case 20:    // eBody_GlassWindscreenFront
    case 21:    // eBody_GlassWindscreenRear
    case 22:    // eBody_GlassPanelLeft_bus
    case 23:    // eBody_GlassPanelRight_bus
        return GlassLarge;
    case 38:    // eBody_NumberPlateRear
    case 39:    // eBody_NumberPlateFront
        return NumberPlate;
    case 40:    // eLights_FrontLeft
    case 41:    // eLights_FrontRight
    case 42:    // eLights_RearLeft
    case 43:    // eLights_RearRight
    case 44:    // eLights_SpecialSiren
    case 121:   // eVariation_Spotlights1
    case 122:   // eVariation_SpotLights2
        return Lights;
    case 45:    // eChassis_FrontEnd
    case 46:    // eChassis_PassengerCell
    case 47:    // eChassis_RearEnd
        return Body;
    case 48:    // eChassis_ArmFrontRight
    case 49:    // eChassis_ArmFrontLeft
    case 50:    // eChassis_ArmRearRight
    case 51:    // eChassis_ArmRearLeft
    case 52:    // eChassis_ArmAdditionalR_Truck
    case 53:    // eChassis_ArmAdditionalL_Truck
        return Suspension;
    case 84:    // eAncillaries_ExhaustSystem1
    case 85:    // eAncillaries_ExhaustSystem2
        return Exhaust;
    case 87:    // eWheels_FrontLeft
    case 88:    // eWheels_FrontRight
    case 89:    // eWheels_RearLeft
    case 90:    // eWheels_RearRight
    case 91:    // eWHEEL
    case 94:    // eWheels_AdditionalRight_Truck
    case 95:    // eWheels_AdditionalLeft_Truck
    case 118:   // eVariation_SpareWheel
    case 132:   // eWHEEL_BLURRED
        return Wheels;
    case 96:    // eInterior_SeatFrontLeft
    case 97:    // eInterior_SeatFrontRight
    case 98:    // eInterior_SeatRearLeft
    case 99:    // eInterior_SeatRearRight
    case 100:   // eInterior_SeatRearBench
    case 101:   // eInterior_SeatAdditional_bus
        return Seats;
    case 106:   // eInterior_Extinguisher
        return Extinguisher;
    case 107:   // eVariation_RoofRacks
    case 116:   // eVariation_Luggage1
    case 117:   // eVariation_Luggage2
        return RoofRacks;
    case 120:   // eVariation_Ladder
        return Ladder;
    case 127:   // eVariation_Crane
        return Crane;
    case 128:   // eVariation_Mixer
        return Mixer;
    case 129:   // eVariation_Tipper
        return Tipper;
    default:
        return Nothing;
    }
}

// ---------------------------------------------------------------------------
// InputCollision(const CameraInfo&, CollisionStateManager&, const PhysicalCarPartContact&,
//                const LogicInputBuffer&, f32, f32)   sub_826BDCB8 (DWARF h:433)
//
//   0x826BDCE8..0x826BDD30  scrape invalid, regular pipeline, not culled, no priority, action
//                           Collision; mPosition = the point on A; maEntityID = (A, B)
//   0x826BDD34..0x826BDD44  meOrientation from MapPositionToOrientation (A's side, the result not
//                           tested)
//   0x826BDD48..0x826BDD64  a HINGED part's contact is culled and nothing else is set
//   0x826BDD68..0x826BDDCC  one side must be a deformable part or detached wheel (entity types
//                           6 / 7 / 9 / 10, asserted cpp:1320)
//   0x826BDDD0..0x826BDE80  maMaterial[0] = `extsw` the part type's material; maMaterial[1] =
//                           `extsw` the OTHER side's MapEntityIdToMaterial (B when A is the part,
//                           else A), with the vehicle interface's player index
//   0x826BDE84..            the impulse lane from the normal stress and dt, the camera lanes
//                           normalised, exactly as the regular builder
// ---------------------------------------------------------------------------
InputCollision::InputCollision(const CameraInfo& lCamera, CollisionStateManager& lMgr,
                               const BrnPhysics::ContactSpy::PhysicalCarPartContact& lContact,
                               const LogicInputBuffer& lInput, f32 /*lfTimeStamp*/, f32 lfTimeStep)
    : maMaterial{0, 0}
    , maEntityID{{0}, {0}}
    , mfPriorityAddition(0.0f)
    , meAction(AttribSys::Enums::eAction::Collision)
    , meOrientation(AttribSys::Enums::eOrientation::Front)
    , mePipeline(E_REGULAR)
    , mbCull(false)
{
    mScrapeInfo.mbValid = false;
    mPosition = lContact.mPointOnA;
    maEntityID[0] = lContact.mEntityIdA;
    maEntityID[1] = lContact.mEntityIdB;
    MapPositionToOrientation(lMgr.GetFrameInformation().mPlayerTransform, lContact.mPointOnA,
                             lContact.mNormal, lContact.mEntityIdA, lContact.mEntityIdB, lMgr,
                             meOrientation);
    if (lContact.mbIsHinged)
    {
        mbCull = true;
        return;
    }

    const u32 luTypeA = GetEntityOwner(lContact.mEntityIdA);
    const u32 luTypeB = GetEntityOwner(lContact.mEntityIdB);
    CGS_ASSERT((luTypeA == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART ||
                luTypeB == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART) ||
               (luTypeA == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART ||
                luTypeB == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART) ||
               (luTypeA == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL ||
                luTypeB == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL) ||
               (luTypeA == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL ||
                luTypeB == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL),
               "(leIdA == E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || leIdB == E_ENTITYTYPE_RACECAR_DEFORMABLE_PART) || "
               "(leIdA == E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART || leIdB == E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART) || "
               "(leIdA == E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || leIdB == E_ENTITYTYPE_DETACHED_RACECAR_WHEEL) || "
               "(leIdA == E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL || leIdB == E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL)");

    maMaterial[0] = static_cast<u64>(static_cast<s64>(MapBodyPartEnumToMateral(
        static_cast<BrnPhysics::Deformation::EBodyParts>(lContact.meType))));
    const bool lbPartIsA = luTypeA == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART ||
                           luTypeA == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART ||
                           luTypeA == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL ||
                           luTypeA == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL;
    const EntityId lOther = lbPartIsA ? lContact.mEntityIdB : lContact.mEntityIdA;
    maMaterial[1] = static_cast<u64>(static_cast<s64>(MapEntityIdToMaterial(
        lOther, lInput.GetVehicleInterface()->GetPlayerActiveRaceCarIndex(), lInput)));

    maParameter[0] = Splat(NormalisedImpulse(lContact.mNormalStress, lfTimeStep));
    maParameter[1] = CameraDistanceSquared(mPosition, lCamera);
    maParameter[2] = CameraFacingNormalised(mPosition, lCamera);
}

// ---------------------------------------------------------------------------
// InputCollision(const CameraInfo&, CollisionStateManager&, const BrokenJointNotificationEvent&,
//                const LogicInputBuffer&, f32, f32)   sub_826BE108 (DWARF h:442)
// InputCollision(... const DetachedPartNotificationEvent& ...)   sub_826BE250 (DWARF h:451)
//
// The two are instruction-for-instruction the same (0x826BE108 / 0x826BE250): scrape invalid,
// action Detach, Front, the regular pipeline, not culled, no priority; mPosition = the event's point;
// maEntityID = (the vehicle, 0); maMaterial[0] = `extsw` MapEntityIdToMaterial(the vehicle, the
// module input's player index -- RootInputBuffer::GetPlayerActiveRaceCarIndex @0x82694F28, input);
// maMaterial[1] = `extsw` the part's material (NO 0x3000000000 bit -- Hex-Rays prints one, the
// instructions are `extsw ; std` at 0x826BE1C0 / 0x826BE1DC); maParameter[0] = KF_MAX_IMPULSE; the
// camera lanes normalised. The stamp and step are not read.
// ---------------------------------------------------------------------------
InputCollision::InputCollision(const CameraInfo& lCamera, CollisionStateManager& lMgr,
                               const BrnPhysics::Deformation::BrokenJointNotificationEvent& lEvent,
                               const LogicInputBuffer& lInput, f32 /*lfTimeStamp*/, f32 /*lfTimeStep*/)
    : maMaterial{0, 0}
    , maEntityID{{0}, {0}}
    , mfPriorityAddition(0.0f)
    , meAction(AttribSys::Enums::eAction::Detach)
    , meOrientation(AttribSys::Enums::eOrientation::Front)
    , mePipeline(E_REGULAR)
    , mbCull(false)
{
    mScrapeInfo.mbValid = false;
    mPosition = lEvent.mPointOnA;
    maEntityID[1].muValue = 0;
    maEntityID[0] = lEvent.mVehicleId;
    maMaterial[0] = static_cast<u64>(static_cast<s64>(MapEntityIdToMaterial(
        lEvent.mVehicleId, ModuleInput(lMgr).GetPlayerActiveRaceCarIndex(), lInput)));
    maMaterial[1] = static_cast<u64>(static_cast<s64>(MapBodyPartEnumToMateral(lEvent.meType)));
    maParameter[0] = KF_MAX_IMPULSE;
    maParameter[1] = CameraDistanceSquared(mPosition, lCamera);
    maParameter[2] = CameraFacingNormalised(mPosition, lCamera);
}

InputCollision::InputCollision(const CameraInfo& lCamera, CollisionStateManager& lMgr,
                               const BrnPhysics::Deformation::DetachedPartNotificationEvent& lEvent,
                               const LogicInputBuffer& lInput, f32 /*lfTimeStamp*/, f32 /*lfTimeStep*/)
    : maMaterial{0, 0}
    , maEntityID{{0}, {0}}
    , mfPriorityAddition(0.0f)
    , meAction(AttribSys::Enums::eAction::Detach)
    , meOrientation(AttribSys::Enums::eOrientation::Front)
    , mePipeline(E_REGULAR)
    , mbCull(false)
{
    mScrapeInfo.mbValid = false;
    mPosition = lEvent.mPointOnA;
    maEntityID[1].muValue = 0;
    maEntityID[0] = lEvent.mVehicleId;
    maMaterial[0] = static_cast<u64>(static_cast<s64>(MapEntityIdToMaterial(
        lEvent.mVehicleId, ModuleInput(lMgr).GetPlayerActiveRaceCarIndex(), lInput)));
    maMaterial[1] = static_cast<u64>(static_cast<s64>(MapBodyPartEnumToMateral(lEvent.meType)));
    maParameter[0] = KF_MAX_IMPULSE;
    maParameter[1] = CameraDistanceSquared(mPosition, lCamera);
    maParameter[2] = CameraFacingNormalised(mPosition, lCamera);
}

// ---------------------------------------------------------------------------
// InputCollision(CollisionStateManager&, EBodyParts, EntityId, const GenericEntity&, EeAction, f32)
//   sub_826BDF60 (DWARF h:460)
//
// A hinged part: scrape invalid, no priority, the given action (HingeOpen / HingeClose / Hinging),
// Front, the regular pipeline, not culled; mPosition = the vehicle's position; maMaterial[0] =
// `extsw` the part's material, maMaterial[1] = `extsw` MapEntityIdToMaterial(the vehicle, the module
// input's player index, the module input); maEntityID[0] = the vehicle with the part as its part
// index (CgsSceneManager::EntityId::SetPartIndex, asserted < 1 << 10), maEntityID[1] = the vehicle;
// maParameter[0] = splat |velocity|; the camera lanes from the manager's camera, NOT normalised.
// ---------------------------------------------------------------------------
InputCollision::InputCollision(CollisionStateManager& lMgr, BrnPhysics::Deformation::EBodyParts leBodyPart,
                               EntityId lEntityId, const GenericEntity& lEntity,
                               AttribSys::Enums::eAction::eAction leAction, f32 lfVelocity)
    : maMaterial{0, 0}
    , maEntityID{{0}, {0}}
    , mfPriorityAddition(0.0f)
    , meAction(leAction)
    , meOrientation(AttribSys::Enums::eOrientation::Front)
    , mePipeline(E_REGULAR)
    , mbCull(false)
{
    mScrapeInfo.mbValid = false;
    mPosition = lEntity.mPosition;
    maMaterial[0] = static_cast<u64>(static_cast<s64>(MapBodyPartEnumToMateral(leBodyPart)));
    const LogicInputBuffer& lrInput = ModuleInput(lMgr);
    maMaterial[1] = static_cast<u64>(static_cast<s64>(MapEntityIdToMaterial(
        lEntityId, lrInput.GetPlayerActiveRaceCarIndex(), lrInput)));
    CgsSceneManager::EntityId lPartId(lEntityId.muValue);
    lPartId.SetPartIndex(static_cast<u32>(leBodyPart));
    maEntityID[0].muValue = lPartId;
    maEntityID[1] = lEntityId;
    maParameter[0] = Splat(std::fabs(lfVelocity));
    maParameter[1] = CameraDistanceSquared(mPosition, lMgr.mCameraInfo);
    maParameter[2] = CameraFacingRaw(mPosition, lMgr.mCameraInfo);
}

// ---------------------------------------------------------------------------
// InputCollision(CollisionStateManager&, const GlassSmashOrCrackEvent&, const GenericEntity&)
//   sub_826BE398 (DWARF h:466)
//
// A glass pane: scrape invalid, mPosition = the vehicle's position, the regular pipeline, no priority,
// action Collision, Front, not culled; meNewState asserted < NUM_GLASS_STATES (cpp:1525);
// maMaterial[0] = `extsw` MapEntityIdToMaterial(the vehicle, the module input's player index),
// maMaterial[1] = `extsw` the pane's material; maEntityID[0] = the vehicle with meNewState (the
// event's +0xA8, `lwz r28, 0xA8(r30)` -- the new state, not the pane) as its part index,
// maEntityID[1] = the vehicle; maParameter[0] = KF_MAX_IMPULSE; the camera lanes from the manager's
// camera, not normalised. Only a SMASH that asked for its effect sounds -- anything else is culled
// ("Glass: Don't play anything but smash.") -- and never an AI car's ("Glass: AI cars don't
// play.", `cmpldi 4`).
// ---------------------------------------------------------------------------
InputCollision::InputCollision(CollisionStateManager& lMgr,
                               const BrnPhysics::Deformation::GlassSmashOrCrackEvent& lEvent,
                               const GenericEntity& lEntity)
    : maMaterial{0, 0}
    , maEntityID{{0}, {0}}
    , mfPriorityAddition(0.0f)
    , meAction(AttribSys::Enums::eAction::Collision)
    , meOrientation(AttribSys::Enums::eOrientation::Front)
    , mePipeline(E_REGULAR)
    , mbCull(false)
{
    mScrapeInfo.mbValid = false;
    mPosition = lEntity.mPosition;
    CGS_ASSERT(lEvent.meNewState < BrnPhysics::Deformation::NUM_GLASS_STATES,
               "lEvent.meNewState < BrnPhysics::Deformation::NUM_GLASS_STATES");
    const LogicInputBuffer& lrInput = ModuleInput(lMgr);
    maMaterial[0] = static_cast<u64>(static_cast<s64>(MapEntityIdToMaterial(
        lEvent.mVehicleEntityId, lrInput.GetPlayerActiveRaceCarIndex(), lrInput)));
    maMaterial[1] = static_cast<u64>(static_cast<s64>(MapBodyPartEnumToMateral(lEvent.meGlassPart)));
    CgsSceneManager::EntityId lPartId(lEvent.mVehicleEntityId.muValue);
    lPartId.SetPartIndex(static_cast<u32>(lEvent.meNewState));
    maEntityID[0].muValue = lPartId;
    maEntityID[1] = lEvent.mVehicleEntityId;
    maParameter[0] = KF_MAX_IMPULSE;
    maParameter[1] = CameraDistanceSquared(lEntity.mPosition, lMgr.mCameraInfo);
    maParameter[2] = CameraFacingRaw(lEntity.mPosition, lMgr.mCameraInfo);
    if (lEvent.meNewState != BrnPhysics::Deformation::E_GLASS_STATE_SMASHED || lEvent.mbDontPlaySmashEffect)
        mbCull = true;
    if (maMaterial[0] == static_cast<u64>(AttribSys::Enums::eMaterialType::AiCar))
        mbCull = true;
}

// ---------------------------------------------------------------------------
// CollisionStateManager::UpdateGlass(const DeformationOutputInterface&)  @ 0x826D4850  (DWARF cpp:3458)
//
// Every glass event of the deformation output (+0x1AF0, asserted cpp:3569) whose vehicle FindEntity
// knows -- looked up only when it differs from the last vehicle seen (`cmpw` with the cached id,
// -1 at first; one GenericEntity local for the whole walk) -- becomes a glass InputCollision.
// ---------------------------------------------------------------------------
void CollisionStateManager::UpdateGlass(const BrnPhysics::Deformation::DeformationOutputInterface& lrDeformation)
{
    const BrnPhysics::Deformation::DeformationOutputInterface::GlassSmashOrCrackQueue* lpQueue =
        &lrDeformation.mGlassSmashOrCrackQueue;
    CGS_ASSERT(lpQueue != nullptr, "lpQueue");
    const u32 lu32Count = static_cast<u32>(lpQueue->GetLength());
    u32 lu32LastVehicle = 0xFFFFFFFFu;
    GenericEntity lEntity;
    for (u32 lu32Index = 0; lu32Index < lu32Count; ++lu32Index)
    {
        const BrnPhysics::Deformation::GlassSmashOrCrackEvent& lrEvent =
            lpQueue->GetEvent(static_cast<s32>(lu32Index));
        if (lu32LastVehicle == lrEvent.mVehicleEntityId.muValue ||
            FindEntity(lrEvent.mVehicleEntityId, lEntity))
        {
            lu32LastVehicle = lrEvent.mVehicleEntityId.muValue;
            InputCollision lCollision(*this, lrEvent, lEntity);
            AddInputCollision(lCollision);
        }
    }
}

// ---------------------------------------------------------------------------
// CollisionStateManager::UpdateHingingBodyParts(const JointedPartStateQueue*)  @ 0x826D44D0
//   (DWARF cpp:3078)
//
// The deformation output's hinged parts (+0x74, asserted cpp:3187), against mHingeCache (aged first
// with the manager's clock). Per event: the orientation clamped to [0, 1] (two fsel -- a NaN gives 1)
// is OPEN within KF_HINGE_OPEN_CLOSED_EPSILON of 1 and CLOSED within it of 0. A part the cache does
// not hold is inserted; one it holds, whose vehicle FindEntity knows (looked up only on a new vehicle;
// one GenericEntity local), sounds:
//   it just opened (was not open)   -> HingeOpen  with the new velocity
//   else it just closed             -> HingeClose with the new velocity
//   else |new - old velocity| > KF_HINGING_VELOCITY_DELTA_TOLERANCE and |new| or |old| >
//        KF_HINGING_VELOCITY_MIN_TOLERANCE   -> Hinging with the larger magnitude (fsel)
// and in every case (FindEntity failing included) the node takes the event, the two flags and the
// clock (asserted valid, cpp:3290).
// ---------------------------------------------------------------------------
void CollisionStateManager::UpdateHingingBodyParts(
    const BrnPhysics::Deformation::DeformationOutputInterface::JointedPartStateQueue* lpQueue)
{
    CGS_ASSERT(lpQueue != nullptr, "lpQueue");
    mHingeCache.Update(mfCurrentTime);

    const u32 lu32Count = static_cast<u32>(lpQueue->GetLength());
    u32 lu32LastVehicle = 0xFFFFFFFFu;
    GenericEntity lEntity;
    for (u32 lu32Index = 0; lu32Index < lu32Count; ++lu32Index)
    {
        const BrnPhysics::Deformation::JointedPartStateEvent& lrEvent =
            lpQueue->GetEvent(static_cast<s32>(lu32Index));

        f32 lfOrientation = lrEvent.mfCurrentOrientation;
        lfOrientation = (-lfOrientation >= 0.0f) ? 0.0f : lfOrientation;
        lfOrientation = (1.0f - lfOrientation >= 0.0f) ? lfOrientation : 1.0f;
        const bool lbClosed = std::fabs(lfOrientation) < KF_HINGE_OPEN_CLOSED_EPSILON;
        const bool lbOpen = std::fabs(lfOrientation - 1.0f) < KF_HINGE_OPEN_CLOSED_EPSILON;

        HingeStateCache::CacheNode* lpCached = mHingeCache.FindInCache(lrEvent);
        if (!lpCached)
        {
            lpCached = mHingeCache.Insert(lrEvent);
        }
        else
        {
            const f32 lfNewVelocity = lrEvent.mfHingeVelocity;
            const f32 lfOldVelocity = lpCached->mEvent.mfHingeVelocity;
            const bool lbWasOpen = lpCached->mbHingeOpen;
            const bool lbWasClosed = lpCached->mbHingeClose;
            const f32 lfVelocityChange = lfNewVelocity - lfOldVelocity;
            if (lu32LastVehicle == lpCached->mEvent.mVehicleId.muValue ||
                FindEntity(lpCached->mEvent.mVehicleId, lEntity))
            {
                lu32LastVehicle = lpCached->mEvent.mVehicleId.muValue;
                if (lbWasOpen != lbOpen && !lbWasOpen)
                {
                    InputCollision lCollision(*this, lpCached->mEvent.meType, lpCached->mEvent.mVehicleId,
                                              lEntity, AttribSys::Enums::eAction::HingeOpen, lfNewVelocity);
                    AddInputCollision(lCollision);
                }
                else if (lbWasClosed != lbClosed && !lbWasClosed)
                {
                    InputCollision lCollision(*this, lpCached->mEvent.meType, lpCached->mEvent.mVehicleId,
                                              lEntity, AttribSys::Enums::eAction::HingeClose, lfNewVelocity);
                    AddInputCollision(lCollision);
                }
                else if (std::fabs(lfVelocityChange) > KF_HINGING_VELOCITY_DELTA_TOLERANCE &&
                         (std::fabs(lfNewVelocity) > KF_HINGING_VELOCITY_MIN_TOLERANCE ||
                          std::fabs(lfOldVelocity) > KF_HINGING_VELOCITY_MIN_TOLERANCE))
                {
                    const f32 lfLarger = (std::fabs(lfNewVelocity) - std::fabs(lfOldVelocity) >= 0.0f)
                                             ? std::fabs(lfNewVelocity) : std::fabs(lfOldVelocity);
                    InputCollision lCollision(*this, lpCached->mEvent.meType, lpCached->mEvent.mVehicleId,
                                              lEntity, AttribSys::Enums::eAction::Hinging, lfLarger);
                    AddInputCollision(lCollision);
                }
            }
        }

        if (lpCached)
        {
            CGS_ASSERT(lpCached->mbValid, "lpCached->mbValid");
            lpCached->mEvent = lrEvent;
            lpCached->mbHingeOpen = lbOpen;
            lpCached->mbHingeClose = lbClosed;
            lpCached->mfTimeLastSeen = mfCurrentTime;
        }
    }
}

void CollisionStateManager::AddInputCollision(const InputCollision& lrCollision)
{
    // ARTIST 0x826D3CF0: default (unfiltered) developer settings. The distance
    // splat at 0x830085A0 is 4900, initialized at 0x82C632B8 from 0x820B94B4.
    const Vector3& lrCamera = mCameraInfo.mTransform.Pos();
    const f32 lfX = lrCollision.mPosition.x - lrCamera.x;
    const f32 lfY = lrCollision.mPosition.y - lrCamera.y;
    const f32 lfZ = lrCollision.mPosition.z - lrCamera.z;
    if (lfX * lfX + lfY * lfY + lfZ * lfZ > 4900.0f)
        return;

    // The console makes room by culling, then drops the new collision if still
    // full. It does not assert on a busy collision frame.
    if (mu32InputCollisionCount >= 64u)
        CullInputCollisions();
    if (mu32InputCollisionCount < 64u)
        maInputCollision[mu32InputCollisionCount++] = lrCollision;
}

// ARTIST 0x826A00C8. Keep the strongest contact for an entity pair and orientation.
// Only regular contacts permit reversing the pair. Match the console's pointer
// worklist and swap-removal order, including retaining the earlier entry on ties.
void CollisionStateManager::CullInputCollisions_RemoveDuplicates()
{
    if (mu32InputCollisionCount <= 1u)
        return;
    InputCollision* lapCollisions[64];
    u32 luCount = 0;
    for (u32 luIndex = 0; luIndex < mu32InputCollisionCount; ++luIndex)
        if (!maInputCollision[luIndex].mbCull)
            lapCollisions[luCount++] = &maInputCollision[luIndex];

    for (u32 luI = 0; luI < luCount; ++luI)
    {
        InputCollision* lpWinner = lapCollisions[luI];
        u32 luJ = luI + 1;
        while (luJ < luCount)
        {
            InputCollision* lpOther = lapCollisions[luJ];
            const bool lbSame = lpWinner->maEntityID[0].muValue == lpOther->maEntityID[0].muValue
                             && lpWinner->maEntityID[1].muValue == lpOther->maEntityID[1].muValue;
            const bool lbReverse = lpWinner->maEntityID[0].muValue == lpOther->maEntityID[1].muValue
                                && lpWinner->maEntityID[1].muValue == lpOther->maEntityID[0].muValue;
            if ((lbSame || (lbReverse && lpOther->mePipeline == InputCollision::E_REGULAR))
                && lpWinner->meOrientation == lpOther->meOrientation)
            {
                const VecFloat& a = lpWinner->maParameter[0];
                const VecFloat& b = lpOther->maParameter[0];
                if (b.x > a.x || b.y > a.y || b.z > a.z || b.w > a.w)
                {
                    lpWinner->mbCull = true;
                    lpWinner = lpOther;
                }
                else
                    lpOther->mbCull = true;
                lapCollisions[luJ] = lapCollisions[--luCount];
            }
            else
                ++luJ;
        }
    }
}

// ARTIST 0x826BE5E0. Suppress repeated sounds during their 0.3-second window,
// unless a regular collision is over ten times stronger than the playing one.
void CollisionStateManager::CullAgainstPlaying()
{
    for (u32 luIndex = 0; luIndex < mu32InputCollisionCount; ++luIndex)
    {
        InputCollision& lrInput = maInputCollision[luIndex];
        for (CgsSound::Logic::State* lpBase = GetHeadState(); lpBase; lpBase = lpBase->GetNextState())
        {
            if (!lpBase->IsAttached())
                continue;
            const CollisionState* lpState = static_cast<const CollisionState*>(lpBase);
            const OutputCollision& lrPlaying = lpState->GetOutputCollision();
            const bool lbRecent = lpState->GetTimeWeAttached() + 0.3f > mfCurrentTime;
            if (lrPlaying.meFatality == E_FATAL_START && lbRecent)
            {
                lrInput.mbCull = true;
                break;
            }
            const bool lbRegular = lrInput.mePipeline == InputCollision::E_REGULAR;
            const bool lbMaterials =
                (lrInput.maMaterial[0] == lrPlaying.maMaterial[0] && lrInput.maMaterial[1] == lrPlaying.maMaterial[1])
                || (lbRegular && lrInput.maMaterial[0] == lrPlaying.maMaterial[1] && lrInput.maMaterial[1] == lrPlaying.maMaterial[0]);
            const bool lbEntities =
                (lrInput.maEntityID[0].muValue == lrPlaying.maEntityID[0].muValue && lrInput.maEntityID[1].muValue == lrPlaying.maEntityID[1].muValue)
                || (lbRegular && lrInput.maEntityID[0].muValue == lrPlaying.maEntityID[1].muValue && lrInput.maEntityID[1].muValue == lrPlaying.maEntityID[0].muValue);
            if (!lbMaterials || !lbEntities
                || mFrameInformation.meFatality.GetCurrent() != lrPlaying.meFatality
                || mFrameInformation.meImpactTime.GetCurrent() != lrPlaying.meImpactTime)
                continue;
            if (lrInput.meAction == AttribSys::Enums::eAction::Collision && lrInput.mePipeline != InputCollision::E_PROP)
            {
                // 0x82C63208 initializes 0x830082E0 from 0x820ABB14 == 0.1.
                const VecFloat& a = lrInput.maParameter[0];
                const VecFloat& b = lrPlaying.maParameter[0];
                if (a.x * 0.1f > b.x && a.y * 0.1f > b.y && a.z * 0.1f > b.z && a.w * 0.1f > b.w)
                    continue;
            }
            if (lrInput.meOrientation == lrPlaying.meOrientation && lbRecent)
            {
                lrInput.mbCull = true;
                break;
            }
        }
    }
}

// ARTIST 0x826BE910: compact in reverse index order by replacing with the tail.
void CollisionStateManager::CullInputCollisions()
{
    CullInputCollisions_RemoveDuplicates();
    CullAgainstPlaying();
    u8 lauRemoved[64];
    u32 luRemoved = 0;
    for (u32 luIndex = 0; luIndex < mu32InputCollisionCount; ++luIndex)
    {
        CGS_ASSERT(luIndex < 64u, "lu32I < E_MAX_COLLISION");
        if (maInputCollision[luIndex].mbCull)
            lauRemoved[luRemoved++] = static_cast<u8>(luIndex);
    }
    while (luRemoved)
    {
        const u32 luIndex = lauRemoved[--luRemoved];
        --mu32InputCollisionCount;
        if (luIndex != mu32InputCollisionCount)
            maInputCollision[luIndex] = maInputCollision[mu32InputCollisionCount];
    }
}

// SKF32_CAMERA_ASPECT_RATIO_NORMAL (DWARF cpp:82) -- unk_830085F0 = splat(4/3), written by the CRT
// thunk 0x82C63278..0x82C63290 (lvlx 0x820AA254 = 0x3FAAAAAB ; vspltw 0 ; stvx128).
// SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN (DWARF cpp:83) -- unk_83005F30 = splat(16/9), CRT
// 0x82C63298..0x82C632B0 (lvlx 0x820AA250 = 0x3FE38E39). The CRT order follows the declaration order.
VecFloat CollisionStateManager::SKF32_CAMERA_ASPECT_RATIO_NORMAL = { 4.0f / 3.0f, 4.0f / 3.0f, 4.0f / 3.0f, 4.0f / 3.0f };
VecFloat CollisionStateManager::SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN = { 16.0f / 9.0f, 16.0f / 9.0f, 16.0f / 9.0f, 16.0f / 9.0f };

// ---------------------------------------------------------------------------
// CollisionStateManager::SetCameraInfo  @0x8269FFD0 (UpdateResolver 0x826F9340)
//   0x8269FFEC..0x826A0024  the camera's four transform rows -> mCameraInfo (+0x8140)
//   0x826A002C..0x826A0030  mfFieldOfView (+0x8180) = the camera's FOV (+0x58)
//   0x826A0034..0x826A004C  mfCosineHalfFov (+0x8184) = the cosine of FOV * flt_82002518 (0x3C0EFA35,
//                           pi/360): the product is single (`fmuls`), the cosine the DOUBLE `cos`
//                           @0x82C096A0, rounded once (`frsp`)
//   0x826A0050..0x826A0094  mfAspectRatio (+0x8188) = E_FLAG_WIDESCREEN of the camera's CURRENT flag
//                           set (`ld 0x140 ; clrldi 63`) ? SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN :
//                           _NORMAL, lane x (`lfs 0(r11)`) -- the camera's own mfAspectRatio (+0x5C)
//                           is not read
//   0x826A0098..0x826A00A8  mfZoom (+0x818C) = GetZoomFromFOVDegs(the camera's FOV)
// Nothing on the console reads the four scalars back: every instruction on +0x8180..+0x818C is here,
// and the builders read the transform's rows 2 and 3 only.
// ---------------------------------------------------------------------------
void CollisionStateManager::SetCameraInfo(
    const BrnDirector::Camera::Camera& lrCamera)
{
    mCameraInfo.mTransform = lrCamera.GetTransform();
    mCameraInfo.mfFieldOfView = lrCamera.GetFOV();
    mCameraInfo.mfCosineHalfFov = static_cast<f32>(std::cos(
        static_cast<f64>(mCameraInfo.mfFieldOfView * 0.0087266462f)));
    mCameraInfo.mfAspectRatio =
        lrCamera.GetState().IsFlagSet(BrnDirector::Camera::CameraState::E_FLAG_WIDESCREEN)
            ? SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN.x
            : SKF32_CAMERA_ASPECT_RATIO_NORMAL.x;
    mCameraInfo.mfZoom = BrnDirector::Camera::Utils::GetZoomFromFOVDegs(
        mCameraInfo.mfFieldOfView);
}

u32 CollisionStateManager::MapCameraStateToBinFlags(
    const BrnDirector::Camera::Camera& lrCamera) const
{
    const BrnDirector::Camera::CameraState& lrState = lrCamera.GetState();
    u32 luFlags = 0;
    if (lrState.IsFlagSet(0))  luFlags |= 0x001u;
    if (lrState.IsFlagSet(3))  luFlags |= 0x002u;
    if (lrState.IsFlagSet(4))  luFlags |= 0x004u;
    if (lrState.IsFlagSet(5))  luFlags |= 0x008u;
    if (lrState.IsFlagSet(7))  luFlags |= 0x010u;
    if (lrState.IsFlagSet(8))  luFlags |= 0x020u;
    if (lrState.IsFlagSet(9))  luFlags |= 0x040u;
    if (lrState.IsFlagSet(10)) luFlags |= 0x080u;
    if (lrState.IsFlagSet(11)) luFlags |= 0x100u;
    if (lrState.IsFlagSet(12)) luFlags |= 0x200u;
    return luFlags ? luFlags : 1u;
}

u32 CollisionStateManager::MapGameModesToBinFlags(const void* lpGameMode) const
{
    if (!lpGameMode)
        return 1u;

    const BrnSound::Module::Io::RootInputBuffer::GameModeOutputInterface*
        lpInterface = static_cast<const
            BrnSound::Module::Io::RootInputBuffer::GameModeOutputInterface*>(
                lpGameMode);
    // The +8 word is now named (BrnRootSoundModuleIo.h): meCurrentGameModeType. The
    // switch below is unchanged -- it was already this field, reached by raw offset.
    const s32 liState = lpInterface->miCurrentGameModeType;
    switch (liState)
    {
    case 0:  return 0x0002u;
    case 2:
    case 16: return 0x0004u;
    case 3:  return 0x0008u;
    case 4:  return 0x0010u;
    case 5:  return 0x0020u;
    case 6:  return 0x0040u;
    case 8:  return 0x0080u;
    case 9:  return 0x0100u;
    case 10: return 0x0200u;
    case 11: return 0x0400u;
    case 12:
    case 14:
    case 17: return 0x0800u;
    case 7:  return 0x1000u;
    case 13: return 0x2000u;
    case 15: return 0x8000u;
    default: return 0x0001u;
    }
}

// ---------------------------------------------------------------------------
// CollisionStateManager::ImportContactSpies<SpyQueue>  (DWARF cpp:1509; ARTIST 0x826DD090
// RaceCarContact [export hole, ppcdis], 0x826DD128 TrafficContact, 0x826DD1C0 DiscardedContact,
// 0x826EB490 PropContact):
//   lCameraInfo = mCameraInfo (`addis r26, this, 1 ; addi -0x7EC0`); lu32SpyCount = length
//   (`lwz r28, 8(queue)`); for each record: InputCollision lCollision(lCameraInfo, *this, lSpy,
//   lInputBuffer, lfTimeStamp, lfTimeStep) -> AddInputCollision(lCollision).
// The overload the record type picks is the console's builder: a PropContact the prop one, a
// DiscardedContact the discarded one (sub_826BDAE8), every other contact the regular one.
// ---------------------------------------------------------------------------
template <typename SpyQueue>
void CollisionStateManager::ImportContactSpies(const SpyQueue& lSpyQueue,
                                               const LogicInputBuffer& lInputBuffer,
                                               f32 lfTimeStamp, f32 lfTimeStep)
{
    const CameraInfo& lCameraInfo = mCameraInfo;
    const u32 lu32SpyCount = static_cast<u32>(lSpyQueue.GetLength());
    for (u32 lu32I = 0; lu32I < lu32SpyCount; ++lu32I)
    {
        InputCollision lCollision(lCameraInfo, *this, lSpyQueue.GetEvent(static_cast<s32>(lu32I)),
                                  lInputBuffer, lfTimeStamp, lfTimeStep);
        AddInputCollision(lCollision);
    }
}

void CollisionStateManager::UpdateResolver(
    const BrnSound::Module::Io::RootInputBuffer& lrInput,
    const BrnSound::Logic::FrameInformation& lrFrame,
    f32 afDeltaTime)
{
    // 0x826F8F64..0x826F8F84 -- right after the three input getters, on EVERY call (no branch
    // around them): both bin lookup caches are rebuilt from their lists, exactly as
    // ResourcesAreReady first built them (+0x98 <- +0x8234, +0x4A0 <- +0x8244).
    maBinLoopupCache[InputCollision::E_REGULAR]
        .Build<Attrib::Gen::crashbinlist, Attrib::Gen::crashbin>(mCrashBinList);
    maBinLoopupCache[InputCollision::E_PROP]
        .Build<Attrib::Gen::propscrashbinlist, Attrib::Gen::propscrashbin>(mPropsCrashBinList);

    // 0x826F9238..0x826F92CC: the frame copy (mgr+0x81A0 <- the frame). The console makes it just
    // after the race-car cache update below; the two touch disjoint state, so it stays here at the
    // head. The director camera is a member of the input (GetDirectorCamera @0x82694B38 returns its
    // address) -- there is no null test on the console.
    mFrameInformation = lrFrame;
    const BrnDirector::Camera::Camera& lrCamera = *lrInput.GetDirectorCamera();

    mx32CameraBinFlags = MapCameraStateToBinFlags(lrCamera);                          // 0x826F9180
    mx32GameModeBinFlags = MapGameModesToBinFlags(lrInput.GetGameModeInterface());    // 0x826F919C

    // 0x826F91E4..0x826F9234: the race-car cache is refreshed from the vehicle interface only while
    // the deformation output carries a state (`lwz r30, 0x70(deform) ; cmplwi ; beq`). It is what
    // MapPositionToOrientation reads for every contact's orientation.
    const BrnPhysics::Deformation::DeformationState* lpDeformationState =
        lrInput.GetDeformationInterface().mpDeformationState;
    if (lpDeformationState)
        mRaceCarCache.Update(*lrInput.GetVehicleInterface(), *lpDeformationState);

    // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): a race car entering the cache, with
    // the transform and deformed box every contact's orientation will be read from.
    if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
    {
        static u32 suCachePrintCount = 0;
        for (u32 luCar = 0; luCar < RaceCarCache::KU_MAX_NUM_RACE_CARS && suCachePrintCount < 16u; ++luCar)
        {
            const RaceCarCache::RaceCarCacheNode& lrCar = *mRaceCarCache.GetRaceCar(luCar);
            if (!lrCar.mbActive.HasChangedTo(true))
                continue;
            ++suCachePrintCount;
            char lacLine[224];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[collision-audio] race-car cache car=%u active pos=(%.2f,%.2f,%.2f) "
                          "box=(%.2f,%.2f,%.2f)..(%.2f,%.2f,%.2f) com=(%.2f,%.2f,%.2f)\n",
                          luCar, lrCar.mTransform.GetCurrent().Pos().x, lrCar.mTransform.GetCurrent().Pos().y,
                          lrCar.mTransform.GetCurrent().Pos().z, lrCar.mMin.x, lrCar.mMin.y, lrCar.mMin.z,
                          lrCar.mMax.x, lrCar.mMax.y, lrCar.mMax.z,
                          lrCar.mComOffset.x, lrCar.mComOffset.y, lrCar.mComOffset.z);
            *CgsDev::Log::gpDebugPrint << lacLine;
        }
    }

    // 0x826F92D0..0x826F9328, on the frame just copied: any change of the impact time (either way,
    // `lwz 4 / lwz 0 ; cmpw ; bne` at +0x81E8) or the fatality turning to E_FATAL_START (+0x81E0,
    // `cmpw ; beq` then `cmpwi 2`) forgets every scrape in the history (`stb 0` to each slot's
    // mbValid, +0x1E69 stride 0x30).
    if (mFrameInformation.meImpactTime.HasChanged() ||
        mFrameInformation.meFatality.HasChangedTo(E_FATAL_START))
    {
        for (u32 lu32Index = 0; lu32Index < E_MAX_SCRAPE_HISTORY; ++lu32Index)
            maScrapeHistory[lu32Index].mbValid = false;
    }

    mu32InputCollisionCount = 0;                                                      // 0x826F933C
    SetCameraInfo(lrCamera);                                                          // 0x826F9340

    // 0x826F9344..0x826F9504: with contact data bound, each queue goes through ImportContactSpies
    // stamped with the manager's current time (`lfs f1, 4(r31)`) and the frame step. Console order:
    // race cars (data+0), traffic (+0x70A0), the discarded contacts (+0x193C0) -- then the glass,
    // hinging body-part, broken-joint, detached-part and car-part legs -- then the props (+0x167E0).
    // (Each group sits behind a developer filter -- dword_82FFB920 / B92C / B928 / B924, zero .bss
    // with no writer.)
    const BrnPhysics::ContactSpy::ContactSpyInterface& lrContacts =
        lrInput.GetContactSpyQueueInterface();
    s32 liPropCount = 0;
    if (lrContacts.IsValid())
    {
        ImportContactSpies(*lrContacts.GetRaceCarContacts(), lrInput, mfCurrentTime, afDeltaTime);
        ImportContactSpies(*lrContacts.GetTrafficContacts(), lrInput, mfCurrentTime, afDeltaTime);

        // 0x826F93C4..0x826F9408: the discarded contacts, through the inlined
        // ContactSpyInterface::GetDiscardedContacts ("mpData != NULL", h:219, then mpData + 0x193C0),
        // one builder each (sub_826BDAE8). CONSOLE FACT: the queue is empty on every frame. Its only
        // producer is PhysicsModule::BridgeSimulationToOutput draining VehicleManager::mDiscardedContacts
        // (0x825B055C..0x825B05EC), and nothing in the image appends to that one -- every instruction on
        // its seats is Construct's bind (0x8263BFA0..0x8263C060), a Clear (PrepareData 0x82633710,
        // DoCrashPrediction 0x82646198) or that drain. The leg runs as the console runs it, over nothing.
        const u32 lu32DiscardedFrom = mu32InputCollisionCount;
        ImportContactSpies(*lrContacts.GetDiscardedContacts(), lrInput, mfCurrentTime, afDeltaTime);
        // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): proves the leg is dispatched and
        // counts what it carried -- the first three frames, every 1800th after that (8 lines), and any
        // frame whose queue is not empty (16 lines).
        if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
        {
            static u32 su32DiscardedFrames = 0;
            static u32 su32DiscardedRecords = 0;
            static u32 su32DiscardedPeriodicLines = 0;
            static u32 su32DiscardedFilledLines = 0;
            const s32 liDiscarded = lrContacts.GetDiscardedContacts()->GetLength();
            ++su32DiscardedFrames;
            su32DiscardedRecords += static_cast<u32>(liDiscarded);
            bool lbPrint = false;
            if (liDiscarded != 0 && su32DiscardedFilledLines < 16u)
            {
                ++su32DiscardedFilledLines;
                lbPrint = true;
            }
            else if ((su32DiscardedFrames <= 3u || su32DiscardedFrames % 1800u == 0u) &&
                     su32DiscardedPeriodicLines < 8u)
            {
                ++su32DiscardedPeriodicLines;
                lbPrint = true;
            }
            if (lbPrint)
            {
                char lacLine[192];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[collision-audio] discarded leg dispatched frame=%u n=%d added=%u total_records=%u\n",
                              su32DiscardedFrames, liDiscarded, mu32InputCollisionCount - lu32DiscardedFrom,
                              su32DiscardedRecords);
                *CgsDev::Log::gpDebugPrint << lacLine;
            }
        }

        // The deformation legs (item 3), on the deformation output the sound input carries (the
        // third input getter, r16): glass 0x826F9420, hinged parts (+0x74) 0x826F9438, broken joints
        // (+0x9F0) 0x826F9450, detached parts (+0x3A0) 0x826F9468, then ContactSpyData's physical
        // car-part contacts (+0x106C0, the inlined GetPhysicalCarPartContacts "mpData != NULL"
        // tripwire, h:195) 0x826F94B0.
        const BrnPhysics::Deformation::DeformationOutputInterface& lrDeformation =
            lrInput.GetDeformationInterface();
        // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): the inputs each deformation leg adds
        // (a budget PER LEG, so a burst on one leg cannot hide the others), the raw queue census on any
        // frame one of them is non-empty, and each glass event's state and suppression flag -- the
        // glass builder culls everything but an unsuppressed smash (0x826BE544..0x826BE564).
        const bool lbDeformDiag = CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint != nullptr;
        // Per leg: up to 8 culled inputs and, separately, up to 24 live ones.
        struct LegBudget { u32 mu32Culled; u32 mu32Live; };
        auto DiagLeg = [this, lbDeformDiag](const char* lpcLeg, u32 lu32From, LegBudget& lrBudget)
        {
            if (!lbDeformDiag)
                return;
            for (u32 lu32Index = lu32From; lu32Index < mu32InputCollisionCount; ++lu32Index)
            {
                const InputCollision& lrAdded = maInputCollision[lu32Index];
                u32& lru32Budget = lrAdded.mbCull ? lrBudget.mu32Culled : lrBudget.mu32Live;
                if (lru32Budget >= (lrAdded.mbCull ? 8u : 24u))
                    continue;
                ++lru32Budget;
                char lacLine[192];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[collision-audio] deform leg=%s action=%d mat=%llx/%llx cull=%d impulse=%g\n",
                              lpcLeg, static_cast<s32>(lrAdded.meAction),
                              static_cast<unsigned long long>(lrAdded.maMaterial[0]),
                              static_cast<unsigned long long>(lrAdded.maMaterial[1]),
                              lrAdded.mbCull ? 1 : 0, static_cast<double>(lrAdded.maParameter[0].x));
                *CgsDev::Log::gpDebugPrint << lacLine;
            }
        };
        if (lbDeformDiag)
        {
            const s32 liGlass = static_cast<s32>(lrDeformation.mGlassSmashOrCrackQueue.GetLength());
            const s32 liHinge = static_cast<s32>(lrDeformation.mJointedPartStateQueue.GetLength());
            const s32 liJoint = static_cast<s32>(lrDeformation.mBrokenJointNotificationQueue.GetLength());
            const s32 liDetached = static_cast<s32>(lrDeformation.mDetachedPartNotificationQueue.GetLength());
            const s32 liCarPart = static_cast<s32>(lrContacts.GetPhysicalCarPartContacts()->GetLength());
            static u32 suCensusCount = 0;
            if ((liGlass | liHinge | liJoint | liDetached | liCarPart) != 0 && suCensusCount < 48u)
            {
                ++suCensusCount;
                char lacLine[160];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[collision-audio] deform queues glass=%d hinge=%d joint=%d detached=%d carpart=%d\n",
                              liGlass, liHinge, liJoint, liDetached, liCarPart);
                *CgsDev::Log::gpDebugPrint << lacLine;
            }
            static u32 su32CrackEventCount = 0;
            static u32 su32SmashEventCount = 0;
            for (s32 liIndex = 0; liIndex < liGlass; ++liIndex)
            {
                const BrnPhysics::Deformation::GlassSmashOrCrackEvent& lrEvent =
                    lrDeformation.mGlassSmashOrCrackQueue.GetEvent(liIndex);
                u32& lru32EventBudget = lrEvent.meNewState == BrnPhysics::Deformation::E_GLASS_STATE_SMASHED
                                            ? su32SmashEventCount : su32CrackEventCount;
                if (lru32EventBudget >= 16u)
                    continue;
                ++lru32EventBudget;
                char lacLine[160];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[collision-audio] glass event vehicle=%08x part=%d state=%d dontplay=%d crack=%g\n",
                              static_cast<unsigned>(lrEvent.mVehicleEntityId.muValue),
                              static_cast<s32>(lrEvent.meGlassPart), static_cast<s32>(lrEvent.meNewState),
                              lrEvent.mbDontPlaySmashEffect ? 1 : 0, static_cast<double>(lrEvent.mfCrackAmount));
                *CgsDev::Log::gpDebugPrint << lacLine;
            }
        }
        static LegBudget sGlassBudget = {0, 0}, sHingeBudget = {0, 0}, sJointBudget = {0, 0};
        static LegBudget sDetachedBudget = {0, 0}, sCarPartBudget = {0, 0};
        u32 lu32LegStart = mu32InputCollisionCount;
        UpdateGlass(lrDeformation);
        DiagLeg("glass", lu32LegStart, sGlassBudget);
        lu32LegStart = mu32InputCollisionCount;
        UpdateHingingBodyParts(&lrDeformation.mJointedPartStateQueue);
        DiagLeg("hinge", lu32LegStart, sHingeBudget);
        lu32LegStart = mu32InputCollisionCount;
        ImportContactSpies(lrDeformation.mBrokenJointNotificationQueue, lrInput, mfCurrentTime, afDeltaTime);
        DiagLeg("joint", lu32LegStart, sJointBudget);
        lu32LegStart = mu32InputCollisionCount;
        ImportContactSpies(lrDeformation.mDetachedPartNotificationQueue, lrInput, mfCurrentTime, afDeltaTime);
        DiagLeg("detached", lu32LegStart, sDetachedBudget);
        lu32LegStart = mu32InputCollisionCount;
        ImportContactSpies(*lrContacts.GetPhysicalCarPartContacts(), lrInput, mfCurrentTime, afDeltaTime);
        DiagLeg("carpart", lu32LegStart, sCarPartBudget);

        liPropCount = lrContacts.GetPropContacts()->GetLength();
        ImportContactSpies(*lrContacts.GetPropContacts(), lrInput, mfCurrentTime, afDeltaTime);
    }

    UpdateScrapes(lrFrame);                                                           // 0x826F9720
    CullInputCollisions();                                                            // 0x826F9728
    ProcessCollisions();                                                              // 0x826F9730
    if (mu32InputCollisionCount != 0u && CollisionAudioDiagEnabled() &&
        CgsDev::Log::gpDebugPrint)
    {
        static u32 suPrintCount = 0;
        if (suPrintCount++ < 32u)
        {
            *CgsDev::Log::gpDebugPrint
                << "[collision-audio] resolve inputs="
                << static_cast<s32>(mu32InputCollisionCount)
                << " outputs=" << static_cast<s32>(mu32OutputCollisionCount)
                << " props=" << liPropCount << " orient=";
            for (u32 luInput = 0; luInput < mu32InputCollisionCount && luInput < 4u; ++luInput)
                *CgsDev::Log::gpDebugPrint << (luInput ? "," : "")
                                           << static_cast<s32>(maInputCollision[luInput].meOrientation);
            *CgsDev::Log::gpDebugPrint << "\n";
        }
    }
}

bool CollisionStateManager::ProcessCollision(
    OutputCollision& lrOutput, const InputCollision& lrInput)
{
    lrOutput = OutputCollision();
    lrOutput.mePipeline = lrInput.mePipeline;
    lrOutput.maMaterial[0] = lrInput.maMaterial[0];
    lrOutput.maMaterial[1] = lrInput.maMaterial[1];
    lrOutput.maEntityID[0] = lrInput.maEntityID[0];
    lrOutput.maEntityID[1] = lrInput.maEntityID[1];
    lrOutput.mPosition = lrInput.mPosition;
    lrOutput.meAction = lrInput.meAction;
    lrOutput.meOrientation = lrInput.meOrientation;
    lrOutput.maParameter[0] = lrInput.maParameter[0];
    lrOutput.maParameter[1] = lrInput.maParameter[1];
    lrOutput.maParameter[2] = lrInput.maParameter[2];
    lrOutput.mScrapeInfo = lrInput.mScrapeInfo;
    lrOutput.mfPriority = lrInput.mfPriorityAddition;
    lrOutput.meFatality = mFrameInformation.meFatality.GetCurrent();
    lrOutput.meImpactTime = mFrameInformation.meImpactTime.GetCurrent();

    switch (lrInput.mePipeline)
    {
    case InputCollision::E_REGULAR:
        SelectCollisionBin<Attrib::Gen::crashbinlist, Attrib::Gen::crashbin>(
            lrOutput, mCrashBinList);
        break;
    case InputCollision::E_PROP:
        SelectCollisionBin<Attrib::Gen::propscrashbinlist, Attrib::Gen::propscrashbin>(
            lrOutput, mPropsCrashBinList);
        break;
    default:
        CGS_ASSERT(false, "lrInput.mePipeline < InputCollision::E_MAX_PIPELINES");
        return false;
    }
    return true;
}

void CollisionStateManager::ProcessCollisions()
{
    mu32OutputCollisionCount = 0;
    for (u32 luIndex = 0;
         luIndex < mu32InputCollisionCount && mu32OutputCollisionCount < 64u;
         ++luIndex)
    {
        if (ProcessCollision(maOutputCollision[mu32OutputCollisionCount],
                             maInputCollision[luIndex]))
            ++mu32OutputCollisionCount;
    }
}

bool CollisionStateManager::LessThanPriority(
    const OutputCollision* lpLeft, const OutputCollision* lpRight)
{
    return lpLeft->mfPriority < lpRight->mfPriority;
}

CgsSound::Logic::State* CollisionStateManager::GetFreeState(void* apvAttachment)
{
    CGS_ASSERT(apvAttachment != nullptr, "lpCollision");
    if (!apvAttachment)
        return nullptr;

    for (CgsSound::Logic::State* lpBase = GetHeadState(); lpBase;
         lpBase = lpBase->GetNextState())
    {
        if (!lpBase->IsAttached())
            return lpBase;
    }

    CollisionState* lpLowestPriority = nullptr;
    for (CgsSound::Logic::State* lpBase = GetHeadState(); lpBase;
         lpBase = lpBase->GetNextState())
    {
        CollisionState* lpState = static_cast<CollisionState*>(lpBase);
        if (!lpLowestPriority ||
            lpState->GetOutputCollision().mfPriority <
                lpLowestPriority->GetOutputCollision().mfPriority)
            lpLowestPriority = lpState;
    }

    const OutputCollision& lrIncoming =
        *static_cast<const OutputCollision*>(apvAttachment);
    if (lpLowestPriority &&
        lrIncoming.mfPriority > lpLowestPriority->GetOutputCollision().mfPriority &&
        lpLowestPriority->Detach())
        return lpLowestPriority;
    return nullptr;
}

void CollisionStateManager::UpdateParams(f32 afDeltaTime)
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    CGS_ASSERT(lpModule != nullptr, "lpSoundLogicModule");
    if (lpModule)
    {
        BrnSound::Module::Io::LogicInputBuffer* lpInput =
            lpModule->GetBrnInputStructure();
        if (lpInput &&
            lpInput->GetPlayerActiveRaceCarIndex() !=
                E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            UpdateResolver(*lpInput, lpModule->GetFrameInformation(), afDeltaTime);

            OutputCollision* lapCollisions[64] = {};
            for (u32 luIndex = 0; luIndex < mu32OutputCollisionCount; ++luIndex)
                lapCollisions[luIndex] = &maOutputCollision[luIndex];
            std::sort(lapCollisions,
                      lapCollisions + mu32OutputCollisionCount,
                      &CollisionStateManager::LessThanPriority);
            for (u32 luIndex = 0; luIndex < mu32OutputCollisionCount; ++luIndex)
            {
                if (!PlayCollision(lapCollisions[luIndex]))
                {
                    // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): the pool had no
                    // free state and no attached one of lower priority, so this output and every
                    // one after it goes unvoiced this frame.
                    if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
                    {
                        static u32 suNoStatePrintCount = 0;
                        if (suNoStatePrintCount++ < 32u)
                        {
                            u32 luAttached = 0;
                            f32 lfLowest = 0.0f;
                            for (CgsSound::Logic::State* lpBase = GetHeadState(); lpBase;
                                 lpBase = lpBase->GetNextState())
                            {
                                const f32 lfPriority = static_cast<CollisionState*>(lpBase)
                                                           ->GetOutputCollision().mfPriority;
                                if (lpBase->IsAttached() && (luAttached++ == 0 || lfPriority < lfLowest))
                                    lfLowest = lfPriority;
                            }
                            const OutputCollision& lrUnplayed = *lapCollisions[luIndex];
                            char lacLine[224];
                            std::snprintf(lacLine, sizeof(lacLine),
                                          "[collision-audio] no free state action=%d mat=%llx/%llx bin=%d "
                                          "priority=%g attached=%u lowest=%g unplayed=%u\n",
                                          static_cast<s32>(lrUnplayed.meAction),
                                          static_cast<unsigned long long>(lrUnplayed.maMaterial[0]),
                                          static_cast<unsigned long long>(lrUnplayed.maMaterial[1]),
                                          static_cast<s32>(lrUnplayed.miBinIndex),
                                          static_cast<double>(lrUnplayed.mfPriority), luAttached,
                                          static_cast<double>(lfLowest),
                                          mu32OutputCollisionCount - luIndex);
                            *CgsDev::Log::gpDebugPrint << lacLine;
                            // The first few saturations: who holds the pool.
                            if (suNoStatePrintCount <= 4u)
                            {
                                for (CgsSound::Logic::State* lpBase = GetHeadState(); lpBase;
                                     lpBase = lpBase->GetNextState())
                                {
                                    if (!lpBase->IsAttached())
                                        continue;
                                    const CollisionState* lpHeld = static_cast<const CollisionState*>(lpBase);
                                    const OutputCollision& lrHeld = lpHeld->GetOutputCollision();
                                    std::snprintf(lacLine, sizeof(lacLine),
                                                  "[collision-audio]   held lifetime=%d priority=%g attached=%.3f now=%.3f "
                                                  "action=%d mat=%llx/%llx bin=%d sample=%d scrape=%d\n",
                                                  static_cast<s32>(lpHeld->GetLifetime().GetCurrent()),
                                                  static_cast<double>(lrHeld.mfPriority),
                                                  static_cast<double>(lpHeld->GetTimeWeAttached()),
                                                  static_cast<double>(mfCurrentTime),
                                                  static_cast<s32>(lrHeld.meAction),
                                                  static_cast<unsigned long long>(lrHeld.maMaterial[0]),
                                                  static_cast<unsigned long long>(lrHeld.maMaterial[1]),
                                                  static_cast<s32>(lrHeld.miBinIndex), lrHeld.miSampleID,
                                                  lrHeld.mScrapeInfo.mbValid ? 1 : 0);
                                    *CgsDev::Log::gpDebugPrint << lacLine;
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    CgsSound::Logic::StateManager::UpdateParams(afDeltaTime);
}

// ---------------------------------------------------------------------------
// BrnSound::Logic::Collision  SelectBin name->bin-index helper  @ 0x826A0598
//
// Shared non-template body both CollisionStateManager::SelectBin<> instantiations
// (SelectBin<crashbinlist,crashbin> and SelectBin<propscrashbinlist,propscrashbin>,
// DWARF h:733) tail-call. Hashes the requested crash-bin content name (a2) with
// CgsSound::Playback::Name::MakeHash and looks it up in a small interned name-hash
// table starting at dword_83005F24.
//
//   Hash = MakeHash(a2);
//   v6 = 0;
//   for ( i = &dword_83005F24; Hash != *i; ++i )
//       if ( ++v6 ) return 1;
//   return v6;
//
// PPC control flow (0x826A05B8..0x826A05D4): the loop-exit `cmplwi v6,1 / blt` can never
// re-enter once v6 has been bumped to 1, so the body runs AT MOST ONCE -- a hit on entry 0
// returns bin index 0 (default); any miss returns bin index 1 (fallback). a1 (`this`, r3)
// and a3/a4/a5 are DEAD in this leaf (the asm forwards ONLY a2 to MakeHash and never
// dereferences `this`); kept in the signature for ABI documentation.
//
// dword_83005F24 RESOLVED (2026-08-25, audio-faithfulness wave 2): the interning
// writer is X360 sub_82C63340 --
//   dword_83005F24 = CgsSound::Playback::Name::MakeHash("CollisionSpliceBank");
// i.e. the single table slot holds the interned hash of the "CollisionSpliceBank"
// content name. Interned here identically at static-init (dynamic initializer over
// the same MakeHash), so a real crash-bin hit on that name returns bin 0 and any
// other name returns fallback bin 1 -- exactly the console behaviour. (The earlier
// placeholder-zero sentinel made EVERY name miss to bin 1.)
// ---------------------------------------------------------------------------
namespace
{
    // Single attested table slot (dword_83005F24), interned exactly as the X360's
    // sub_82C63340 does.
    uintptr_t gauCollisionBinNameHashes[1] =
        { CgsSound::Playback::Name::MakeHash("CollisionSpliceBank") };
}

int SelectBin( int /*a1*/, const char* lkpacName, int /*a3*/, int /*a4*/, int /*a5*/ )
{
    uintptr_t luHash = CgsSound::Playback::Name::MakeHash( lkpacName );

    int luBinIndex = 0;
    for ( uintptr_t* lpuEntry = gauCollisionBinNameHashes; luHash != *lpuEntry; ++lpuEntry )
    {
        if ( ++luBinIndex )
            return 1;
    }
    return luBinIndex;
}

// ---------------------------------------------------------------------------
// CrashBinUtils<CrashBin>::GetSampleIds -- copy an AttribSys crash-bin's
// collision-sample-id array into a caller u16 buffer.
//
//   crashbin      @ 0x8268DC18  (DWARF BrnCollisionStateManager.h:538)
//   propscrashbin @ 0x8268FE90  (DWARF BrnCollisionStateManager.h:604)
//
// Stateless utility MEMBER (not a free function): the container is the explicit
// first arg lpCrashBin; because CrashBinUtils holds no data the method never
// touches its own `this`. lpfnGetArraySize / lpfnGetArrayItem are the bin's
// generated AttribSys array accessors as POINTERS TO MEMBER (DWARF :529/:530
// `{ __pfn, __delta }`), and the X360 leaf invokes them THROUGH lpCrashBin
// (`mr r3,r28 ; mtctr r29 ; bctrl` @0x8268FF24, `mr r3,r28 ; mtctr r27 ; bctrl`
// @0x8268FF68) -- the bin IS dereferenced, as `this` of each accessor; the
// Int32 layout field is a plain 32-bit int living in the crash-bin attribute data
// area, so the accessors return a reference to it. Copies luNumCollisions =
// *lpfnGetArraySize() indices into lpauArray (truncating each to u16), bounded by
// luMaxSize, and returns the count. Asserts collapse to CGS_ASSERT; message
// strings verbatim from X360 rodata, file-path + line args dropped.
//
// The single generic template body below is shared by both instantiations (the
// crashbin/propscrashbin bins differ only in type); the two explicit instantiations
// emit the linker symbols at their X360 addresses.
// ---------------------------------------------------------------------------
template< typename CrashBin >
unsigned int CrashBinUtils< CrashBin >::GetSampleIds(
    const CrashBin*                lpCrashBin,   // r3 (r28) -- `this` of both accessor calls
    const int&        (CrashBin::*lpfnGetArraySize)() const,
    const int&        (CrashBin::*lpfnGetArrayItem)( unsigned int ) const,
    u16*                           lpauArray,
    u16                            luMaxSize )
{
    CGS_ASSERT( lpfnGetArrayItem != 0, "lpGetArrayItem" );
    CGS_ASSERT( lpfnGetArraySize != 0, "lpGetArraySize" );
    CGS_ASSERT( lpauArray        != 0, "lpauArray" );

    unsigned int luNumCollisions = ( lpCrashBin->*lpfnGetArraySize )();

    CGS_ASSERT( luNumCollisions < luMaxSize, "luNumCollisions < luMaxSize" );

    for ( unsigned int i = 0; i < luNumCollisions; ++lpauArray )
    {
        *lpauArray = static_cast<u16>( ( lpCrashBin->*lpfnGetArrayItem )( i++ ) );
    }

    return luNumCollisions;
}

template <typename ListType, typename BinType>
void CollisionStateManager::SelectCollisionBin(
    OutputCollision& lrOutput, const ListType& lrList)
{
    // 0x826A9808..0x826A9820 (propscrash 0x826A8848..0x826A8860): the sample id is cleared first,
    // then a collision whose SECOND material is exactly 1 -- the value MapEntityIdToMaterial gives
    // an unhandled owner -- selects no bin at all (`ld 0x10 ; cmpldi 1 ; beq -> out`).
    lrOutput.miSampleID = -1;
    // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): the early-out below, counted --
    // a second material of Nothing (an entity no material maps, e.g. the prop a race-car contact
    // touched) never reaches a bin; the prop pipeline voices that contact.
    if (lrOutput.maMaterial[1] == 1 && CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
    {
        static u32 suSkipPrintCount = 0;
        if (suSkipPrintCount++ < 32u)
        {
            char lacLine[128];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[collision-audio] skip pipeline=%d second material 1 mat0=0x%016llx\n",
                          static_cast<int>(lrOutput.mePipeline),
                          static_cast<unsigned long long>(lrOutput.maMaterial[0]));
            *CgsDev::Log::gpDebugPrint << lacLine;
        }
    }
    if (lrOutput.maMaterial[1] == 1)
        return;
    const f32 lfDistanceSquared = lrOutput.maParameter[1].x;
    const f32 lfImpulse = lrOutput.maParameter[0].x;

    // NOT IN X360 BINARY: opt-in host diagnostics for auditing the recovered
    // resolver.  Each counter is cumulative, so the first counter that stops
    // advancing identifies the original authored-bin predicate rejecting a hit.
    u32 luValidBins = 0;
    u32 luMaterialBins = 0;
    u32 luCameraBins = 0;
    u32 luGameModeBins = 0;
    u32 luImpactTimeBins = 0;
    u32 luFatalityBins = 0;
    u32 luOrientationBins = 0;
    u32 luActionBins = 0;
    u32 luDistanceBins = 0;
    u32 luImpulseBins = 0;

    // SelectBin<List, Bin> (@0x826A97E8 crashbin / @0x826A8828 propscrashbin): the material pair
    // of every bin comes from THIS PIPELINE'S LOOKUP CACHE, built by ResourcesAreReady --
    //   0x826A987C  mulli r10, mePipeline, 0x408 ; addi 0x98   == &maBinLoopupCache[mePipeline]
    //   0x826A9894  lwz muEntryCount                            == the walk's bound
    //   0x826A9A6C  GetEntry(i) (its "lu32Index < muEntryCount" assert, h:252)
    //   0x826A9A94..0x826A9AE8  forward  (mat[0] & A) && (B & mat[1])
    //                           reverse  (mat[1] & A) && (B & mat[0]), taken only on the REGULAR
    //                           pipeline (0x826A9884 cntlzw -> the `pipeline == 0` byte)
    // and the bin's attributes are resolved only for an entry whose materials match.
    const BinLookupCache& lrCache = maBinLoopupCache[lrOutput.mePipeline];
    const bool lbReverseAllowed = lrOutput.mePipeline == InputCollision::E_REGULAR;
    for (u32 luIndex = 0; luIndex < lrCache.GetEntryCount(); ++luIndex)
    {
        const BinLookupCache::CacheEntry& lrEntry = lrCache.GetEntry(luIndex);
        const bool lbMaterialsForward =
            (lrOutput.maMaterial[0] & lrEntry.mx64MaterialA) != 0 &&
            (lrEntry.mx64MaterialB & lrOutput.maMaterial[1]) != 0;
        const bool lbMaterialsReverse =
            (lrOutput.maMaterial[1] & lrEntry.mx64MaterialA) != 0 &&
            (lrEntry.mx64MaterialB & lrOutput.maMaterial[0]) != 0;
        if (!lbMaterialsForward && !(lbMaterialsReverse && lbReverseAllowed))
            continue;
        ++luMaterialBins;

        BinType lBin(lrList.GetCrashBinCollectionKey(luIndex), nullptr);
        if (!lBin.IsValid())
            continue;
        ++luValidBins;
        if ((lBin.mCameras() & mx32CameraBinFlags) == 0)
            continue;
        ++luCameraBins;
        if ((lBin.mGameModes() & mx32GameModeBinFlags) == 0)
            continue;
        ++luGameModeBins;
        if ((lBin.mImpactTime() & static_cast<u32>(lrOutput.meImpactTime)) == 0)
            continue;
        ++luImpactTimeBins;
        if ((lBin.mFatalityFlag() &
             (1u << static_cast<u32>(lrOutput.meFatality))) == 0)
            continue;
        ++luFatalityBins;
        if ((lBin.mOrientation() &
             static_cast<u32>(lrOutput.meOrientation)) == 0)
            continue;
        ++luOrientationBins;
        if ((lBin.mAction() & static_cast<u32>(lrOutput.meAction)) == 0)
            continue;
        ++luActionBins;
        const f32 lfDistanceMin = lBin.DistanceFactor_Min();
        const f32 lfDistanceMax = lBin.DistanceFactor_Max();
        if (lfDistanceSquared < lfDistanceMin * lfDistanceMin ||
            lfDistanceSquared > lfDistanceMax * lfDistanceMax)
            continue;
        ++luDistanceBins;

        // 0x826AA2B4 `stb r22, 0xD4(r29)`: the bin index is stored as soon as the distance test
        // passes -- before the impulse test can still reject this bin.
        lrOutput.miBinIndex = static_cast<s8>(luIndex);
        if (lfImpulse < lBin.PhysicsImpulseNormalization_MIN())
            continue;
        ++luImpulseBins;

        // 0x826AA320 `std r25, 0x90(r29)`: the bin's collection key, stored before the size test.
        lrOutput.mBinKey = lrList.GetCrashBinCollectionKey(luIndex);

        // 0x826AA324..0x826AA34C: normalise over [MIN, MAX], then clamp with two fsel's --
        //   fneg f13, x ; fsel f0, f13, 0.0, x      -> x <= 0 (incl. -0) gives 0; NaN stays NaN
        //   fsubs f13, 1.0, x ; fsel f30, f13, x, 1.0 -> x > 1, +inf or NaN gives 1
        // There is no guard for MAX == MIN: such a bin normalises to 1.0 (x/0 and 0/0 both end on
        // the second fsel's 1.0). flt_82001CC0 = 0.0f, flt_82001C98 = 1.0f (0x826A9A00 / 0x826A9A08).
        f32 lfNormalized =
            (lfImpulse - lBin.PhysicsImpulseNormalization_MIN()) /
            (lBin.PhysicsImpulseNormalization_MAX() - lBin.PhysicsImpulseNormalization_MIN());
        lfNormalized = (-lfNormalized >= 0.0f) ? 0.0f : lfNormalized;
        lfNormalized = (1.0f - lfNormalized >= 0.0f) ? lfNormalized : 1.0f;

        // 0x826AA38C..0x826AA4F0: the normalised impulse picks ONE size against the bin's
        // IntensityThreshold (lane 1 for large, lane 0 for medium). A bin with no sample of that
        // size is abandoned for the NEXT bin (each of the three arms: `li r11, -1 ;
        // stw r11, 0xD8(r29)`, then the loop's continue) -- there is no fallback to a smaller
        // size inside the same bin.
        ESize leSize;
        s32 liNumSamples;
        if (lfNormalized > lBin.IntensityThreshold().y)                  // vspltw 1 ; vcmpgtfp.
        {
            leSize = E_SIZE_LARGE;                                       // 0x826AA654
            liNumSamples = lBin.mNumCollisionsLarge();                   // 0x826AA3D0 +0x168
        }
        else if (lfNormalized > lBin.IntensityThreshold().x)             // vspltw 0 ; vcmpgtfp.
        {
            leSize = E_SIZE_MEDIUM;                                      // 0x826AA65C
            liNumSamples = lBin.mNumCollisionsMedium();                  // 0x826AA45C +0x164
        }
        else
        {
            leSize = E_SIZE_SMALL;                                       // 0x826AA664
            liNumSamples = lBin.mNumCollisionsSmall();                   // 0x826AA4A8 +0x160
        }
        if (liNumSamples <= 0)
        {
            lrOutput.miSampleID = -1;
            // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG); the console's own
            // BinLogic debug text here is "No large / medium / small samples in this bin, next".
            if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
            {
                static u32 suSizeMissPrintCount = 0;
                if (suSizeMissPrintCount++ < 32u)
                {
                    char lacLine[128];
                    std::snprintf(lacLine, sizeof(lacLine),
                                  "[collision-audio] size miss pipeline=%d entry=%u size=%d "
                                  "normalized=%f -> next bin\n",
                                  static_cast<int>(lrOutput.mePipeline), luIndex,
                                  static_cast<int>(leSize), static_cast<double>(lfNormalized));
                    *CgsDev::Log::gpDebugPrint << lacLine;
                }
            }
            continue;
        }
        lrOutput.meSize = leSize;

        // The accepted bin (0x826AA73C..0x826AA790): the splatted normalised impulse, sample id
        // 0, the bin's content-spec bank, its priority added.
        lrOutput.mNormalizedImpulse =
            VecFloat{lfNormalized, lfNormalized, lfNormalized, lfNormalized};
        lrOutput.miSampleID = 0;
        lrOutput.meBankType = SelectBin(0, lBin.mSpliceBankAsset(), 0, 0, 0);
        CGS_ASSERT(lrOutput.meBankType >= E_COLLISION_SPLICE_BANK_COLLISION &&
                   lrOutput.meBankType < E_COLLISION_SPLICE_BANK_MAX,
                   "leSpliceBankType < E_COLLISION_SPLICE_BANK_MAX");
        lrOutput.mfPriority += lBin.Priority();

        // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): the selection resolved
        // through the pipeline's lookup cache -- which entry, and the pair it matched on. A
        // deformation-leg input (a detach / hinge action, or a body-part / mirror / glass
        // material: 0x20|0x40|0x80|0x100|0x200) keeps its own budget and is tagged.
        if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
        {
            static u32 suCacheSelectPrintCount = 0;
            static u32 suDeformSelectPrintCount = 0;
            const bool lbDeformInput =
                static_cast<s32>(lrOutput.meAction) != 1 ||
                ((lrOutput.maMaterial[0] | lrOutput.maMaterial[1]) & 0x3E0ull) != 0;
            u32& lruSelectBudget = lbDeformInput ? suDeformSelectPrintCount : suCacheSelectPrintCount;
            if (lruSelectBudget++ < 32u)
            {
                char lacLine[192];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[collision-audio] cache select pipeline=%d entry=%u/%u %s "
                              "matA=0x%016llx matB=0x%016llx%s\n",
                              static_cast<int>(lrOutput.mePipeline), luIndex,
                              lrCache.GetEntryCount(),
                              lbMaterialsForward ? "forward" : "reverse",
                              static_cast<unsigned long long>(lrEntry.mx64MaterialA),
                              static_cast<unsigned long long>(lrEntry.mx64MaterialB),
                              lbDeformInput ? " input=deform" : "");
                *CgsDev::Log::gpDebugPrint << lacLine;
            }
        }
        return;
    }

    if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
    {
        // Plain collisions (action 1) and the deformation actions (detach / hinge) keep separate
        // budgets, so a run's early wall hits cannot hide what happened to a door coming off.
        static u32 suRejectPrintCount = 0;
        static u32 suDeformRejectPrintCount = 0;
        u32& lruRejectBudget = static_cast<s32>(lrOutput.meAction) == 1 ? suRejectPrintCount
                                                                         : suDeformRejectPrintCount;
        if (lruRejectBudget++ < 32u)
        {
            *CgsDev::Log::gpDebugPrint
                << "[collision-audio] reject pipeline="
                << static_cast<s32>(lrOutput.mePipeline)
                << " bins=" << static_cast<s32>(lrCache.GetEntryCount())
                << " gates=" << static_cast<s32>(luMaterialBins)
                << "/" << static_cast<s32>(luValidBins)
                << "/" << static_cast<s32>(luCameraBins)
                << "/" << static_cast<s32>(luGameModeBins)
                << "/" << static_cast<s32>(luImpactTimeBins)
                << "/" << static_cast<s32>(luFatalityBins)
                << "/" << static_cast<s32>(luOrientationBins)
                << "/" << static_cast<s32>(luActionBins)
                << "/" << static_cast<s32>(luDistanceBins)
                << "/" << static_cast<s32>(luImpulseBins)
                << " mat=" << static_cast<s32>(lrOutput.maMaterial[0] >> 32)
                << ":" << static_cast<s32>(lrOutput.maMaterial[0] & 0xFFFFFFFFull)
                << "/" << static_cast<s32>(lrOutput.maMaterial[1] >> 32)
                << ":" << static_cast<s32>(lrOutput.maMaterial[1] & 0xFFFFFFFFull)
                << " camera=" << static_cast<s32>(mx32CameraBinFlags)
                << " mode=" << static_cast<s32>(mx32GameModeBinFlags)
                << " impact=" << static_cast<s32>(lrOutput.meImpactTime)
                << " fatal=" << static_cast<s32>(lrOutput.meFatality)
                << " orient=" << static_cast<s32>(lrOutput.meOrientation)
                << " action=" << static_cast<s32>(lrOutput.meAction)
                << " distance2=" << lfDistanceSquared
                << " impulse=" << lfImpulse << "\n";
        }
    }
}

template <typename BinType>
void CollisionStateManager::GetRandomSampleID(OutputCollision& lrOutput)
{
    BinType lBin(lrOutput.mBinKey, nullptr);
    CGS_ASSERT(lBin.IsValid(), "lCrashBin.IsValid()");

    const int& (BinType::*lpGetCount)() const =
        lrOutput.meSize == E_SIZE_LARGE
            ? &BinType::mNumCollisionsLarge
            : (lrOutput.meSize == E_SIZE_MEDIUM
                ? &BinType::mNumCollisionsMedium
                : &BinType::mNumCollisionsSmall);
    const int& (BinType::*lpGetItem)(u32) const =
        lrOutput.meSize == E_SIZE_LARGE
            ? &BinType::mCollisionsLarge
            : (lrOutput.meSize == E_SIZE_MEDIUM
                ? &BinType::mCollisionsMedium
                : &BinType::mCollisionsSmall);

    u16 lauSampleIds[32] = {};
    CrashBinUtils<BinType> lUtils;
    const u16 luNumSamples = static_cast<u16>(lUtils.GetSampleIds(
        &lBin, lpGetCount, lpGetItem, lauSampleIds, 32u));
    if (luNumSamples == 0)
    {
        lrOutput.miSampleID = -1;
        return;
    }

    CGS_ASSERT(lrOutput.meBankType >= E_COLLISION_SPLICE_BANK_COLLISION &&
               lrOutput.meBankType < E_COLLISION_SPLICE_BANK_MAX,
               "leSpliceBankType < E_COLLISION_SPLICE_BANK_MAX");
    CgsSound::Utils::SelectionHistory<512, u16, u16, 65536>& lrHistory =
        maSelectionHistory[lrOutput.meBankType];
    const u16 luSampleId =
        lrHistory.FindRandomOldest<u16, 32>(lauSampleIds, luNumSamples);
    lrHistory.Update(luSampleId);
    lrOutput.miSampleID = static_cast<s32>(luSampleId);
}

// Explicit instantiations (the two crash-bin specialisations the X360 build emits).
template struct CrashBinUtils< Attrib::Gen::crashbin >;
template struct CrashBinUtils< Attrib::Gen::propscrashbin >;

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
