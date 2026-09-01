#include "GameSource/Sound/Collision/BrnCollisionStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"
#include "GameSource/Sound/Collision/BrnCollisionState.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"

#include <algorithm>
#include <cmath>
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
//   state 1: SetCollisionBinList(this) (seed the per-material crash-bin lists);
//            Content::Construct + LoadAsset the crash splicer banks
//            (MakeHash the crash content names);
//   state 2: if (!Content::IsLoaded(...)) return 0;
//            mCpuMonitor = PerfMonCpu::AddMonitor("Collisions", 14, 0, 1.0, ...)
//   state 3: if (!StateManager::PrepareStates(...)) return 0;
//   state 4: return 1;
//
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
        mePrepareState = E_PREPARE_UPDATING;
        if (!mbResourcesAreLoaded ||
            !mCollisionSplicerBank[E_COLLISION_SPLICE_BANK_COLLISION].IsLoaded() ||
            !mScrapesCsisInterface.IsLoaded() ||
            !mScrapesAemsBank.IsLoaded())
            return false;
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
// (IResourceRequester completion callback)
//
// ---------------------------------------------------------------------------
void CollisionStateManager::ResourcesAreReady()
{
    if (!mbResourcesAreLoaded)
    {
        CgsSound::Logic::Module* lpModule = GetLogicModule();
        const u32 luAemsFactory = static_cast<u32>(
            CgsSound::Playback::AemsFactorySkName().GetValue());
        const u32 luSplicerFactory = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("~SplicerFactory::SK_NAME~"));

        mScrapesCsisInterface.Construct(
            lpModule, luAemsFactory,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("ScrapesCsis")));
        mScrapesAemsBank.Construct(
            lpModule, luAemsFactory,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("ScrapePatchBank.abi")));
        mCollisionSplicerBank[E_COLLISION_SPLICE_BANK_COLLISION].Construct(
            lpModule, luSplicerFactory,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("CollisionSpliceBank")));
        mbResourcesAreLoaded = true;
    }

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
        return 1;

    if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
    {
        static u32 suPrintCount = 0;
        if (suPrintCount++ < 32u)
        {
            *CgsDev::Log::gpDebugPrint
                << "[collision-audio] play pipeline="
                << static_cast<s32>(lpCollision->mePipeline)
                << " bin=" << static_cast<s32>(lpCollision->miBinIndex)
                << " size=" << static_cast<s32>(lpCollision->meSize)
                << " sample=" << lpCollision->miSampleID
                << " impulse=" << lpCollision->mNormalizedImpulse.x << "\n";
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

u64 CollisionStateManager::MapEntityIdToMaterial(
    const EntityId& lrEntityId,
    const BrnSound::Module::Io::RootInputBuffer& lrInput) const
{
    const u32 luOwner = lrEntityId.muValue >> 24;
    const u32 luIndex = (lrEntityId.muValue >> 10) & 0x3FFFu;
    switch (luOwner)
    {
    case 0u: // world
        return 0x10ull;
    case 1u: // race car
        return luIndex == static_cast<u32>(
            lrInput.GetPlayerActiveRaceCarIndex()) ? 0x2ull : 0x4ull;
    case 2u: // traffic
    {
        const BrnSound::Module::Io::RootInputBuffer::PhysicalTrafficStateQueue*
            lpTraffic = lrInput.GetPhysicalTrafficStates();
        if (lpTraffic)
        {
            for (s32 liIndex = 0; liIndex < lpTraffic->GetLength(); ++liIndex)
            {
                const BrnPhysics::Vehicle::PhysicalTrafficState& lrState =
                    lpTraffic->GetEvent(liIndex);
                if (lrState.mEntityID.muValue != lrEntityId.muValue)
                    continue;
                if (lrState.mbIsFatallyCrashing)
                    return 0x1000000ull;
                if (lrState.mbIsDeforming)
                    return 0x800000ull;
                break;
            }
        }
        return 0x8ull;
    }
    default:
        return 0x1ull;
    }
}

void CollisionStateManager::MakeBaseInputCollision(
    InputCollision& lrOut,
    const BrnPhysics::ContactSpy::BaseContact& lrContact,
    const BrnSound::Module::Io::RootInputBuffer& lrInput,
    f32 afDeltaTime) const
{
    lrOut = InputCollision();
    lrOut.mePipeline = InputCollision::E_REGULAR;
    lrOut.maEntityID[0] = lrContact.mEntityIdA;
    lrOut.maEntityID[1] = lrContact.mEntityIdB;
    lrOut.mPosition = lrContact.mPointOnA;
    lrOut.maMaterial[0] = MapEntityIdToMaterial(lrOut.maEntityID[0], lrInput);
    // InputCollision::InputCollision @ 0x826BDAE8 ORs this authored category
    // into the second material after MapEntityIdToMaterial.  The crash-bin
    // material pairs include that bit; omitting it prevents every regular
    // collision from reaching a bin.
    lrOut.maMaterial[1] =
        MapEntityIdToMaterial(lrOut.maEntityID[1], lrInput) |
        0x2000000000ull;

    // ARTIST flt_830083E0: the stress-to-per-second normalisation used by both
    // regular and prop InputCollision constructors.
    // ARTIST computes this static from KF_FASTEST_COLLISION (200) and
    // KF_BIGGEST_THING_MASS (1600) during global initialization.
    static const f32 KF_BIGGEST_COLLISION_IN_SECOND = 320000.0f;
    const f32 lfDt = std::max(afDeltaTime, 0.000001f);
    const f32 lfStress = std::sqrt(
        lrContact.mNormalStress.x * lrContact.mNormalStress.x +
        lrContact.mNormalStress.y * lrContact.mNormalStress.y +
        lrContact.mNormalStress.z * lrContact.mNormalStress.z);
    const f32 lfImpulse =
        lfStress / (KF_BIGGEST_COLLISION_IN_SECOND * lfDt);
    lrOut.maParameter[0] = VecFloat{lfImpulse, lfImpulse, lfImpulse, lfImpulse};

    const Vector3 lToContact{
        lrOut.mPosition.x - mCameraInfo.mTransform.Pos().x,
        lrOut.mPosition.y - mCameraInfo.mTransform.Pos().y,
        lrOut.mPosition.z - mCameraInfo.mTransform.Pos().z,
        0.0f};
    const f32 lfDistanceSquared =
        lToContact.x * lToContact.x + lToContact.y * lToContact.y +
        lToContact.z * lToContact.z;
    lrOut.maParameter[1] = VecFloat{lfDistanceSquared, lfDistanceSquared,
                                    lfDistanceSquared, lfDistanceSquared};
    f32 lfFacing = 0.0f;
    if (lfDistanceSquared > 0.0f)
    {
        const f32 lfInvDistance = 1.0f / std::sqrt(lfDistanceSquared);
        lfFacing = (lToContact.x * lrContact.mNormal.x +
                    lToContact.y * lrContact.mNormal.y +
                    lToContact.z * lrContact.mNormal.z) * lfInvDistance;
    }
    lrOut.maParameter[2] = VecFloat{lfFacing, lfFacing, lfFacing, lfFacing};
}

void CollisionStateManager::MakePropInputCollision(
    InputCollision& lrOut,
    const BrnPhysics::ContactSpy::PropContact& lrContact,
    const BrnSound::Module::Io::RootInputBuffer& lrInput,
    f32 afDeltaTime) const
{
    MakeBaseInputCollision(lrOut, lrContact, lrInput, afDeltaTime);
    lrOut.mePipeline = InputCollision::E_PROP;
    lrOut.maMaterial[1] =
        MapEntityIdToMaterial(lrContact.mEntityIdB, lrInput);
    lrOut.mbCull = !MapPropTypeToMaterial(lrContact.muType,
                                          lrOut.maMaterial[0]);
    if (lrContact.muBeganMoving == 1u)
        lrOut.mfPriorityAddition = 1.0f;
}

void CollisionStateManager::AddInputCollision(const InputCollision& lrCollision)
{
    if (lrCollision.mbCull)
        return;
    CGS_ASSERT(mu32InputCollisionCount < 64u,
               "mu32InputCollisionCount < KU_MAX_INPUT_COLLISIONS");
    if (mu32InputCollisionCount < 64u)
        maInputCollision[mu32InputCollisionCount++] = lrCollision;
}

void CollisionStateManager::SetCameraInfo(
    const BrnDirector::Camera::Camera& lrCamera)
{
    mCameraInfo.mTransform = lrCamera.GetTransform();
    mCameraInfo.mfFieldOfView = lrCamera.GetFOV();
    mCameraInfo.mfCosineHalfFov = std::cos(
        mCameraInfo.mfFieldOfView * 0.0087266462f);
    mCameraInfo.mfAspectRatio = lrCamera.mfAspectRatio;
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
    s32 liState = 0;
    std::memcpy(&liState, lpInterface->mData + 8u, sizeof(liState));
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

void CollisionStateManager::UpdateResolver(
    const BrnSound::Module::Io::RootInputBuffer& lrInput,
    const BrnSound::Logic::FrameInformation& lrFrame,
    f32 afDeltaTime)
{
    mFrameInformation = lrFrame;
    const BrnDirector::Camera::Camera* lpCamera = lrInput.GetDirectorCamera();
    CGS_ASSERT(lpCamera != nullptr, "lpDirectorCamera");
    if (!lpCamera)
        return;

    mx32CameraBinFlags = MapCameraStateToBinFlags(*lpCamera);
    mx32GameModeBinFlags = MapGameModesToBinFlags(lrInput.GetGameModeInterface());
    SetCameraInfo(*lpCamera);
    mu32InputCollisionCount = 0;

    const BrnPhysics::ContactSpy::ContactSpyInterface& lrContacts =
        lrInput.GetContactSpyQueueInterface();
    s32 liPropCount = 0;
    if (lrContacts.IsValid())
    {
        const BrnPhysics::ContactSpy::ContactSpyData::RaceCarContactQueue*
            lpRaceCars = lrContacts.GetRaceCarContacts();
        for (s32 liIndex = 0; lpRaceCars && liIndex < lpRaceCars->GetLength(); ++liIndex)
        {
            InputCollision lCollision;
            MakeBaseInputCollision(lCollision,
                *lpRaceCars->GetBaseContact(liIndex), lrInput, afDeltaTime);
            AddInputCollision(lCollision);
        }

        const BrnPhysics::ContactSpy::ContactSpyData::TrafficContactQueue*
            lpTraffic = lrContacts.GetTrafficContacts();
        for (s32 liIndex = 0; lpTraffic && liIndex < lpTraffic->GetLength(); ++liIndex)
        {
            InputCollision lCollision;
            MakeBaseInputCollision(lCollision,
                *lpTraffic->GetBaseContact(liIndex), lrInput, afDeltaTime);
            AddInputCollision(lCollision);
        }

        const BrnPhysics::ContactSpy::ContactSpyData::PropContactQueue*
            lpProps = lrContacts.GetPropContacts();
        liPropCount = lpProps ? lpProps->GetLength() : 0;
        for (s32 liIndex = 0; lpProps && liIndex < lpProps->GetLength(); ++liIndex)
        {
            InputCollision lCollision;
            MakePropInputCollision(lCollision,
                lpProps->GetEvent(liIndex), lrInput, afDeltaTime);
            AddInputCollision(lCollision);
        }
    }

    ProcessCollisions();
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
                << " props=" << liPropCount << "\n";
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
                    break;
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
    lrOutput.miSampleID = -1;
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

    for (u32 luIndex = 0; luIndex < lrList.mNumCrashBins(); ++luIndex)
    {
        BinType lBin(lrList.GetCrashBinCollectionKey(luIndex), nullptr);
        if (!lBin.IsValid())
            continue;
        ++luValidBins;

        const bool lbMaterialsForward =
            (lBin.mMaterialA() & lrOutput.maMaterial[0]) != 0 &&
            (lBin.mMaterialB() & lrOutput.maMaterial[1]) != 0;
        const bool lbMaterialsReverse =
            lrOutput.mePipeline == InputCollision::E_REGULAR &&
            (lBin.mMaterialA() & lrOutput.maMaterial[1]) != 0 &&
            (lBin.mMaterialB() & lrOutput.maMaterial[0]) != 0;
        if (!lbMaterialsForward && !lbMaterialsReverse)
            continue;
        ++luMaterialBins;
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
        if (lfImpulse < lBin.PhysicsImpulseNormalization_MIN())
            continue;
        ++luImpulseBins;

        const f32 lfDenominator =
            lBin.PhysicsImpulseNormalization_MAX() -
            lBin.PhysicsImpulseNormalization_MIN();
        const f32 lfNormalized = lfDenominator > 0.0f
            ? std::max(0.0f, std::min(1.0f,
                (lfImpulse - lBin.PhysicsImpulseNormalization_MIN()) /
                    lfDenominator))
            : 0.0f;
        lrOutput.mNormalizedImpulse =
            VecFloat{lfNormalized, lfNormalized, lfNormalized, lfNormalized};

        if (lfNormalized > lBin.IntensityThreshold().y &&
            lBin.mNumCollisionsLarge() > 0)
            lrOutput.meSize = E_SIZE_LARGE;
        else if (lfNormalized > lBin.IntensityThreshold().x &&
                 lBin.mNumCollisionsMedium() > 0)
            lrOutput.meSize = E_SIZE_MEDIUM;
        else if (lBin.mNumCollisionsSmall() > 0)
            lrOutput.meSize = E_SIZE_SMALL;
        else
            continue;

        lrOutput.miSampleID = 0;
        lrOutput.mBinKey = lrList.GetCrashBinCollectionKey(luIndex);
        lrOutput.meBankType = SelectBin(0, lBin.mSpliceBankAsset(), 0, 0, 0);
        CGS_ASSERT(lrOutput.meBankType >= E_COLLISION_SPLICE_BANK_COLLISION &&
                   lrOutput.meBankType < E_COLLISION_SPLICE_BANK_MAX,
                   "leSpliceBankType < E_COLLISION_SPLICE_BANK_MAX");
        lrOutput.miBinIndex = static_cast<s8>(luIndex);
        lrOutput.mfPriority += lBin.Priority();
        return;
    }

    if (CollisionAudioDiagEnabled() && CgsDev::Log::gpDebugPrint)
    {
        static u32 suRejectPrintCount = 0;
        if (suRejectPrintCount++ < 32u)
        {
            *CgsDev::Log::gpDebugPrint
                << "[collision-audio] reject pipeline="
                << static_cast<s32>(lrOutput.mePipeline)
                << " bins=" << static_cast<s32>(lrList.mNumCrashBins())
                << " gates=" << static_cast<s32>(luValidBins)
                << "/" << static_cast<s32>(luMaterialBins)
                << "/" << static_cast<s32>(luCameraBins)
                << "/" << static_cast<s32>(luGameModeBins)
                << "/" << static_cast<s32>(luImpactTimeBins)
                << "/" << static_cast<s32>(luFatalityBins)
                << "/" << static_cast<s32>(luOrientationBins)
                << "/" << static_cast<s32>(luActionBins)
                << "/" << static_cast<s32>(luDistanceBins)
                << "/" << static_cast<s32>(luImpulseBins)
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
