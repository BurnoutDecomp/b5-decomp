#ifndef BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"   // BrnSound::Logic::BrnStateManager (committed base)
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h" // BrnSound::Logic::Collision::ScrapeInfo (committed; maScrapeHistory element)
#include "GameSource/Sound/Collision/BrnBinLookupCache.h"          // BinLookupCache (maBinLoopupCache, DWARF h:787)
#include "GameSource/Sound/Collision/BrnRaceCarCache.h"            // RaceCarCache (mRaceCarCache, DWARF h:790)
#include "GameSource/Sound/Collision/BrnHingeStateCache.h"         // HingeStateCache (mHingeCache, DWARF h:791)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"   // the deformation legs' queues
#include "GameSource/AttribSys/Enums/eMaterialType.h"              // EeMaterialType (MapEntityIdToMaterial)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"       // CgsSound::Playback::Name::MakeHash (SelectBin helper)
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"
#include "GameSource/AttribSys/Generated/classes/crashbin.h"
#include "GameSource/AttribSys/Generated/classes/propscrashbin.h"
#include "GameSource/AttribSys/Generated/classes/crashbinlist.h"
#include "GameSource/AttribSys/Generated/classes/propscrashbinlist.h"
#include "GameSource/AttribSys/Generated/classes/proptomaterialmappings.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"

namespace BrnDirector { namespace Camera { struct Camera; } }
namespace BrnPhysics { namespace ContactSpy { struct BaseContact; struct PropContact; } }
namespace BrnSound { namespace Module { namespace Io { struct RootInputBuffer; } } }

// =============================================================================
// BrnSound::Logic::Collision::CollisionStateManager
//   GameSource/Sound/Collision/BrnCollisionStateManager.{h,cpp}
//   (canonical home -- derived from the X360 mangled name
//    BrnSound::Logic::Collision::CollisionStateManager; the Sound/Collision/ dir
//    already exists in-tree and hosts the sibling collision-audio classes --
//    BrnCollisionDataStructures, BrnCollisionFrameInformation, BrnHingeStateCache.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// CollisionStateManager is the sound-logic state manager that owns the collision /
// crash audio -- BY FAR the largest of the 9 managers (33408 bytes / 0x8280). It
// builds the per-material collision-generator lists, the crash-bin attribute tables,
// and the collision-event splicer banks, then drives the crash voices each frame. It
// is one of the 9 managers the SoundLogicModule factory CreateStateManagers
// (0x826AFEF8) creates via CreateStateMan.
//
// BASE CHAIN: CollisionStateManager : public BrnSound::Logic::BrnStateManager
//   (-> CgsSound::Logic::StateManager primary base + BrnSound::Logic::
//    IResourceRequester sub-object). Evidence: the ctor @ 0x826FFAC0 installs a
//   primary vtable @ +0 (off_820B844C) AND a secondary sub-object vtable @ +0x90
//   (*(a1+144) = off_820B8444, after a transient off_820AB608) -- the
//   IResourceRequester sub-object vptr -- and the dtor @ 0x826FFD48 tears down the
//   base CgsSound::Logic::StateManager::RegisteredContent ObjectPool at +0xC. Same
//   shape as the committed siblings AIVehicleStateManager / PassbyStateManager.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 object is 33408 bytes (0x8280)
// behind 4-byte pointers/vptrs (CreateObject @ 0x82701FA8 allocates 33408); on the
// 64-bit host the layout differs, so members are pinned BY NAME only and the 0x8280
// size / absolute offsets are NOT static_asserted. Runtime members are represented by
// their recovered names and roles rather than console padding or raw-offset access.
// =============================================================================

// The crash-bin classes must be complete before CrashBinUtils is declared. On MSVC x64
// a pointer-to-member of an incomplete class uses the general 24-byte representation,
// while these single-inheritance generated classes use the 8-byte representation. The
// ARTIST helper is called with pointers to their concrete accessors, so both the caller
// and the explicit-instantiation site must compile against the same complete types.

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

enum ECollisionSpliceTags : int
{
    E_COLLISION_SPLICE_SMALL_LANDINGS = 0,
    E_COLLISION_SPLICE_SUSPENSION = 1,
    E_COLLISION_SPLICE_HARD_LANDINGS = 2,
    E_COLLISION_SPLICE_CRASH_LANDINGS = 3,
    E_COLLISION_SPLICE_JUNKYARD_LANDING_SWEETNER = 4,
    E_COLLISION_SPLICE_CRASH_IN_WATER = 5,
};

enum ECollisionSpliceBankType
{
    E_COLLISION_SPLICE_BANK_COLLISION = 0,
    E_COLLISION_SPLICE_BANK_MAX = 1,
};

// ---------------------------------------------------------------------------
// BrnSound::Logic::Collision::CrashBinUtils<CrashBin> -- a STATELESS utility struct
// (DWARF BrnCollisionStateManager.h:528/538) that copies the collision-sample-id
// array out of an AttribSys crash-bin container into a caller u16 buffer.
// GetSampleIds takes the container as an explicit first parameter (the struct holds
// no data, so the method never touches its own `this`). The two accessors are the
// bin's generated array-size / array-item getters and they are POINTERS TO MEMBER
// FUNCTIONS of the bin, invoked THROUGH lpCrashBin: the X360 leaf @0x8268FF24 does
// `mr r3,r28(lpCrashBin) ; mtctr r29 ; bctrl` and @0x8268FF68 the same for the item
// getter, i.e. the bin is `this` for both calls. DWARF :529/:530 renders the two
// parameters as `struct { const Int32& (*)() __pfn; int __delta; }` -- MSVC's
// pointer-to-member representation, not a plain function pointer (a 2026-08-18
// verify caught the earlier `const int& (*)()` spelling: it does not accept
// `&propscrashbin::mNumCollisionsSmall`, C2664). The _LayoutStruct::Int32 field is a
// plain 32-bit int in the attribute data area, modelled as `const int&`.
//
// Explicit instantiations (defined in BrnCollisionStateManager.cpp):
//   CrashBinUtils<Attrib::Gen::crashbin>::GetSampleIds      @ 0x8268DC18
//   CrashBinUtils<Attrib::Gen::propscrashbin>::GetSampleIds @ 0x8268FE90
// ---------------------------------------------------------------------------
template< typename CrashBin >
struct CrashBinUtils
{
    // Copy every collision index the crash-bin container holds
    // (count = *lpfnGetArraySize()) into lpauArray (each item truncated to u16),
    // bounded by luMaxSize; return the count.
    unsigned int GetSampleIds(
        const CrashBin*             lpCrashBin,
        const int&    (CrashBin::*lpfnGetArraySize)() const,
        const int&    (CrashBin::*lpfnGetArrayItem)( unsigned int ) const,
        u16*                        lpauArray,
        u16                         luMaxSize );
};

// Collision-event descriptor is defined in BrnCollisionDataStructures.h.
struct OutputCollision;

struct CameraInfo
{
    CameraInfo()
        : mfFieldOfView(0.0f)
        , mfCosineHalfFov(0.0f)
        , mfAspectRatio(0.0f)
        , mfZoom(0.0f)
    {
        mTransform.SetIdentity();
    }

    Matrix44Affine mTransform;
    f32 mfFieldOfView;
    f32 mfCosineHalfFov;
    f32 mfAspectRatio;
    f32 mfZoom;
};

struct PropToMaterialMapping
{
    PropToMaterialMapping() : muMaterialIndex(0), mbValid(false) {}
    u16 muMaterialIndex;
    bool mbValid;
};

// DWARF BrnCollisionStateManager.h:102. One side of a scrape as the scrape legs see it (FindEntity
// fills it): where the entity is and how fast it moves, whether it is crashing, the player's car,
// the world. The constructor (h:103) zeroes all of it -- both callers build their locals from 0.0f
// (0x82001CC0) and zero bytes (UpdateScrapeHistory 0x826BEBA8..0x826BEC7C, UpdateScrapes
// 0x826D4380..0x826D443C); FindEntity stores +0x00 / +0x10 / +0x20 / +0x21 / +0x22.
struct GenericEntity
{
    GenericEntity()
        : mbCrashing(false)
        , mbPlayer(false)
        , mbWorld(false)
    {
        mPosition.SetZero();
        mVelocity.SetZero();
    }

    Vector3 mPosition;   // h:111
    Vector3 mVelocity;   // h:112
    bool mbCrashing;     // h:113
    bool mbPlayer;       // h:114
    bool mbWorld;        // h:115
};

// SelectBin name->bin-index helper  @ 0x826A0598. Free function (the asm never uses
// its r3 as `this`) -- the shared body both CollisionStateManager::SelectBin<>
// template instantiations tail-call. Hashes the requested crash-bin content name and
// looks it up in a small interned name-hash table; returns bin index 0 (default) on a
// hit at entry 0, else 1 (fallback). See BrnCollisionStateManager.cpp.
int SelectBin( int a1, const char* lkpacName, int a3, int a4, int a5 );

// DWARF: BrnSound::Logic::Collision::EeMaterialType -- the 32-bit material flag every builder
// widens into InputCollision::maMaterial with `extsw`.
typedef AttribSys::Enums::eMaterialType::eMaterialType EeMaterialType;

class CollisionStateManager;

// DWARF BrnCollisionStateManager.cpp:3593, ARTIST @0x826A0CF8. The material an entity collides
// as: the world, the player's car or another race car, a traffic car by its vehicle class, or
// Nothing for any other owner. See BrnCollisionStateManager.cpp.
EeMaterialType MapEntityIdToMaterial( EntityId lEntityId, s32 liPlayerIndex,
                                      const LogicInputBuffer& lInput );

// DWARF BrnCollisionStateManager.cpp:3677, ARTIST @0x82688CF8 (the console's own spelling). The
// material a car body part collides as: large / small body panels, mirrors, small / large glass,
// number plates, lights, the chassis body, suspension, exhaust, wheels, seats, the extinguisher,
// roof racks, the ladder, crane / mixer / tipper; Nothing for any other part.
EeMaterialType MapBodyPartEnumToMateral( BrnPhysics::Deformation::EBodyParts leBodyPart );

// DWARF BrnCollisionStateManager.cpp:190, ARTIST @0x8269ED18. Which face of a car's deformed box
// a contact point is nearest (Front / Rear / Side / Roof / Bottom). The fourth vector is passed
// zero by its only caller and never read (0x8269ED64 overwrites v3 before any use).
AttribSys::Enums::eOrientation::eOrientation MapPositionToOrientationUsingBox(
    Vector3 lPosition, Vector3 lNormal, Matrix44Affine lTransform, Vector3 lUnused,
    Vector3 lComOffset, Vector3 lMin, Vector3 lMax );

// DWARF BrnCollisionStateManager.cpp:318, ARTIST @0x8269F418. The orientation of a contact on
// race car A from the collision manager's race-car cache; false (and Front) when A is not an
// active race car. The leading matrix is passed and never read.
bool MapPositionToOrientation( Matrix44Affine lTransform, Vector3 lPosition, Vector3 lNormal,
                               EntityId lVehicleIdA, EntityId lVehicleIdB,
                               const CollisionStateManager& lMgr,
                               AttribSys::Enums::eOrientation::eOrientation& leOrientation );

class CollisionStateManager : public BrnSound::Logic::BrnStateManager
{
public:
    // CollisionStateManager @ 0x826FFAC0.
    CollisionStateManager();

    // ~CollisionStateManager @ 0x826FFD48 (the X360 `vector deleting destructor`).
    virtual ~CollisionStateManager();

    // ---- RTTI hooks (the per-class descriptor + factory). STATIC GetStaticTypeInfo
    // / CreateObject so &CreateObject is storable in ClassTypeInfo<StateManager>::
    // mpfnCreateObject (the X360 CreateObject @ 0x82701FA8 never touches an instance
    // -- its int arg is the operator-new flavour selector, not `this`). ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );                     // @ 0x82701FA8

    // DWARF BrnCollisionStateManager.cpp:82 / :83 (class statics): the aspect ratio SetCameraInfo
    // gives the collision camera -- 4:3, or 16:9 while the director camera is flagged
    // E_FLAG_WIDESCREEN. .bss splats (unk_830085F0 / unk_83005F30); values in the .cpp.
    static VecFloat SKF32_CAMERA_ASPECT_RATIO_NORMAL;
    static VecFloat SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN;

    // ---- boot + lifecycle virtuals ----
    virtual bool Prepare();                       // @ 0x826F8B78  (vtable +0x0C)
    virtual CgsSound::Logic::State* GetFreeState(void* apvAttachment) override;
    virtual void UpdateParams(f32 afDeltaTime) override;
    virtual void Notify(const CgsSound::Io::MessageHeader* apkMessage) override;

    // ---- IResourceRequester overrides (pure in IResourceRequester; BrnStateManager
    // declares but does not body them, so the concrete leaf must override+body them). ----
    virtual void                            ResourcesAreReady();
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar();

    // FindInScrapeHistory @ 0x826889E0 (DWARF h:880 -- the NON-const overload). Linear
    // scan of the 16-slot scrape history; returns the first VALID slot that compares
    // equal to rScrapeInfo, else nullptr.
    BrnSound::Logic::Collision::ScrapeInfo* FindInScrapeHistory( const BrnSound::Logic::Collision::ScrapeInfo& rScrapeInfo );

    // DWARF h:635 / h:644 (header inlines). MapPositionToOrientation reads the race-car cache
    // (mgr+0xCF0, 0x8269F484) and the builders read the frame copy (mgr+0x81A0 / +0x81E8).
    const RaceCarCache& GetRaceCarCache() const { return mRaceCarCache; }
    const BrnSound::Logic::FrameInformation& GetFrameInformation() const { return mFrameInformation; }

    // DWARF cpp:3210 (public). The prop InputCollision maps its prop type through it
    // (sub_826E8B20 0x826E8D0C).
    bool MapPropTypeToMaterial(u16 luPropType, u64& lruMaterial) const;

    // PlayCollision @ 0x82704028. Non-virtual; called by UpdateParams.
    int PlayCollision( OutputCollision* lpCollision );

    const CgsSound::Logic::Content& GetSplicerBank(
        ECollisionSpliceBankType aeBank) const
    {
        CGS_ASSERT(aeBank >= E_COLLISION_SPLICE_BANK_COLLISION &&
                   aeBank < E_COLLISION_SPLICE_BANK_MAX,
                   "leSpliceBankType < E_COLLISION_SPLICE_BANK_MAX");
        return mCollisionSplicerBank[aeBank];
    }

    const CgsSound::Logic::Content& GetScrapeAemsBank() const
    {
        return mScrapesAemsBank;
    }

private:
    // The InputCollision constructors reach the manager's protected surface: the prop one adds
    // its prop-vs-prop partner straight to the input list (AddInputCollision, sub_826E8B20
    // 0x826E8DFC) and the regular one culls against the scrape history.
    friend struct InputCollision;

    // DWARF cpp:1509 (member template, one instantiation per queue type): build an
    // InputCollision from every record in the queue and add it -- ARTIST 0x826DD090
    // (RaceCarContact), 0x826DD128 (TrafficContact), 0x826EB490 (PropContact).
    template <typename SpyQueue>
    void ImportContactSpies(const SpyQueue& lSpyQueue, const LogicInputBuffer& lInputBuffer,
                            f32 lfTimeStamp, f32 lfTimeStep);

    // The scrape legs of the resolver (DWARF h:827..h:907). UpdateScrapes (cpp:2678, ARTIST
    // 0x826D3F50) runs after every contact is imported and before the culls: it ages the history
    // (UpdateScrapeHistory, cpp:2916, 0x826BEB98), eats a collision that continues a scrape, turns
    // an attached collision state into a scrape (or attaches a new one) and records new scrapes
    // (FindOldestScrapeInHistory, cpp:2881, 0x82688A58). UniqueScrape (cpp:2975, 0x82688B20) keeps
    // one entry per scrape in each worklist; FindEntity (cpp:3000, 0x826A0398) reads one side of a
    // scrape from the sound input.
    enum { E_MAX_SCRAPE_HISTORY = 16 };   // FindOldestScrapeInHistory's assert (cpp:2992) / `cmpwi 0x10`
    void UpdateScrapes(const BrnSound::Logic::FrameInformation& lrFrame);
    BrnSound::Logic::Collision::ScrapeInfo* FindOldestScrapeInHistory();
    bool FindEntity(const EntityId& lEntityId, GenericEntity& lEntity) const;
    bool UniqueScrape(const BrnSound::Logic::Collision::ScrapeInfo& lScrapeInfo,
                      const InputCollision* const* lapCollisions, u32 lu32Count) const;
    void UpdateScrapeHistory(const BrnSound::Logic::FrameInformation& lrFrame);

    // The deformation legs of the resolver: UpdateGlass (DWARF cpp:3458, ARTIST 0x826D4850) turns
    // the deformation output's glass events into collisions, UpdateHingingBodyParts (cpp:3078,
    // 0x826D44D0) its hinged parts' opening / closing / swinging, through mHingeCache.
    void UpdateGlass(const BrnPhysics::Deformation::DeformationOutputInterface& lrDeformation);
    void UpdateHingingBodyParts(
        const BrnPhysics::Deformation::DeformationOutputInterface::JointedPartStateQueue* lpQueue);

    void SetCollisionBinList(u64 luCollisionBinListKey,
                             u64 luPropsCollisionBinListKey,
                             u64 luPropsMappingKey);
    void BuildPropToMaterialTable();
    void UpdateResolver(const BrnSound::Module::Io::RootInputBuffer& lrInput,
                        const BrnSound::Logic::FrameInformation& lrFrame,
                        f32 afDeltaTime);
    void AddInputCollision(const InputCollision& lrCollision);
    void CullInputCollisions();
    void CullInputCollisions_RemoveDuplicates();
    void CullAgainstPlaying();
    bool ProcessCollision(OutputCollision& lrOutput, const InputCollision& lrInput);
    void ProcessCollisions();
    void SetCameraInfo(const BrnDirector::Camera::Camera& lrCamera);
    u32 MapCameraStateToBinFlags(const BrnDirector::Camera::Camera& lrCamera) const;
    u32 MapGameModesToBinFlags(const void* lpGameMode) const;
    static bool LessThanPriority(const OutputCollision* lpLeft,
                                 const OutputCollision* lpRight);

    template <typename ListType, typename BinType>
    void SelectCollisionBin(OutputCollision& lrOutput, const ListType& lrList);

    template <typename BinType>
    void GetRandomSampleID(OutputCollision& lrOutput);

    // DWARF BrnCollisionStateManager.h:787 (the console's own spelling). One material pre-filter
    // per pipeline, indexed by InputCollision::EPipeline: ResourcesAreReady @0x826D3788 builds
    // [E_REGULAR] from mCrashBinList (0x826D37C0) and [E_PROP] from mPropsCrashBinList
    // (0x826D37D0); SelectBin reads [mePipeline] (0x826A987C `mulli 0x408` + 0x98).
    BinLookupCache maBinLoopupCache[InputCollision::E_MAX_PIPELINES];
    CgsSound::Utils::SelectionHistory<512, u16, u16, 65536>
        maSelectionHistory[E_COLLISION_SPLICE_BANK_MAX];
    // DWARF h:790. The eight race cars' cached transform and deformed box (X360 +0xCF0, 8 x 192;
    // the ctor clears only each node's mbActive pair, 0x826FFB1C..0x826FFB58).
    RaceCarCache mRaceCarCache;
    // DWARF h:791 (X360 +0x12F0, 32 x 28). The hinged parts seen lately: their last orientation,
    // velocity and open / closed flags (UpdateHingingBodyParts).
    HingeStateCache mHingeCache;
    PropToMaterialMapping maPropToMaterialMappings[500];
    InputCollision maInputCollision[64];
    OutputCollision maOutputCollision[64];
    CameraInfo mCameraInfo;
    u32 mu32InputCollisionCount;
    u32 mu32OutputCollisionCount;
    BrnSound::Logic::FrameInformation mFrameInformation;

    // DWARF (BrnCollisionStateManager.h:639). The 16-entry scrape history (X360 +0x1E40, stride
    // 48 -- the full ScrapeInfo). UpdateScrapes records new scrapes into it and refreshes the ones
    // that continue; UpdateScrapeHistory ages them out; UpdateResolver forgets them all when the
    // impact time changes or the fatality starts; the regular InputCollision reads it for the
    // SloMoCrash cull.
    BrnSound::Logic::Collision::ScrapeInfo maScrapeHistory[E_MAX_SCRAPE_HISTORY];

    // The three ref-counted content handles built at the tail of the ARTIST
    // constructor.  They are the runtime-visible part of the otherwise deferred
    // collision-manager payload and are required by landing/scrape voices.
    CgsSound::Logic::Content mScrapesCsisInterface;
    CgsSound::Logic::Content mScrapesAemsBank;
    CgsSound::Logic::Content mCollisionSplicerBank[E_COLLISION_SPLICE_BANK_MAX];
    Attrib::Gen::crashbinlist mCrashBinList;
    Attrib::Gen::propscrashbinlist mPropsCrashBinList;
    Attrib::Gen::proptomaterialmappings mPropMaterialMappings;
    bool mbResourcesAreLoaded;
    bool mbBoundToProps;
    CgsResource::ResourceHandle mPropDataResourceHandle;
    u32 mx32CameraBinFlags;
    u32 mx32GameModeBinFlags;
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H
